#include "StructGraphManager.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Field/FieldSystemObjects.h"      
#include "Field/FieldSystemTypes.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Field/FieldSystemComponent.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/UnrealType.h"
#include "NiagaraFunctionLibrary.h"

AStructGraphManager::AStructGraphManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AStructGraphManager::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("[StructGraph] BeginPlay: %s"), *GetName());

	BuildGCCache();

	// Field
	if (!Stage3_GravityField)
	{
		Stage3_GravityField = NewObject<UUniformVector>(this);
	}

	// 시뮬 조절
	ForEachValidGC(GCSlabs, [](UGeometryCollectionComponent* GC)
		{
			PrepAsWalkableKinematicGC(GC);
		});

	//FTimerHandle Tmp;
	//GetWorldTimerManager().SetTimer(Tmp, this, &AStructGraphManager::Debug_ApplyStrainOnce, 0.5f, false);

	ApplySettleDampingThenRestore();

	UE_LOG(LogTemp, Warning, TEXT("[Gravity] WorldGravityZ=%.1f"), GetWorld()->GetGravityZ());
}

void AStructGraphManager::InitializeFromBP(
	const TArray<UStaticMeshComponent*>& InSlabL1,
	const TArray<UStaticMeshComponent*>& InSlabL2,
	const TArray<UStaticMeshComponent*>& InSlabL3,
	const TArray<UStaticMeshComponent*>& InSlabRoof,
	const TArray<UStaticMeshComponent*>& InColumns,
	const TArray<UStaticMeshComponent*>& InWalls)
{
	SlabL1 = InSlabL1;
	SlabL2 = InSlabL2;
	SlabL3 = InSlabL3;
	SlabRoof = InSlabRoof;
	Columns = InColumns;
	Walls = InWalls;

	Nodes.Reset();
	OriginalTransforms.Reset();

	BuildNodes();
	SaveOriginalTransforms();
	BuildSupportGraph();
	BuildDownGraph();

	CalibrateCapacitiesFromInitialState();


	SolveLoadsAndDamage();

	CacheBaseCapacities();


	auto ApplyStabilize = [&](const TArray<TObjectPtr<UStaticMeshComponent>>& Arr)
		{
			for (UStaticMeshComponent* C : Arr)
			{
				StabilizeStructureComponent(C);
			}
		};

	ApplyStabilize(SlabL1);
	ApplyStabilize(SlabL2);
	ApplyStabilize(SlabL3);
	ApplyStabilize(SlabRoof);
	ApplyStabilize(Columns);
	ApplyStabilize(Walls);

	UE_LOG(LogTemp, Warning, TEXT("[StructGraph] Initialized. Nodes=%d, Originals=%d"), Nodes.Num(), OriginalTransforms.Num());
}

void AStructGraphManager::StartEarthquake()
{
	if (bRunning) return;
	bRunning = true;
	Elapsed = 0.f;

	GetWorldTimerManager().SetTimer(
		TickHandle,
		this,
		&AStructGraphManager::TickEarthquake,
		0.05f,
		true
	);
}

void AStructGraphManager::StopEarthquake()
{
	if (!bRunning) return;
	bRunning = false;
	GetWorldTimerManager().ClearTimer(TickHandle);
}

void AStructGraphManager::ResetStructure()
{
	StopEarthquake();
	UE_LOG(LogTemp, Warning, TEXT("[CALL] ResetStructure"));
	for (FStructGraphNode& N : Nodes)
	{
		N.State = EStructDamageState::Intact;
		N.CarriedLoad = 0.f;

		if (N.Comp)
		{
			if (UPrimitiveComponent* PC = N.Comp)
			{
				PC->SetSimulatePhysics(false);
				PC->SetEnableGravity(false);

				if (FTransform* T = OriginalTransforms.Find(PC))
				{
					PC->SetWorldTransform(*T, false, nullptr, ETeleportType::TeleportPhysics);
				}
			}
		}
	}

	SolveLoadsAndDamage();
}

int32 AStructGraphManager::AddNode(EStructNodeType Type, UPrimitiveComponent* Comp, int32 FloorIndex)
{
	FStructGraphNode Node;
	Node.Id = Nodes.Num();
	Node.Type = Type;
	Node.Comp = Comp;
	Node.FloorIndex = FloorIndex;

	if (Comp)
	{
		const FBox Box = GetCompBox(Comp);
		const FVector Size = Box.GetSize();
		const float Volume = FMath::Max(1.f, Size.X * Size.Y * Size.Z);

		const float Density = (Type == EStructNodeType::Slab) ? 0.00008f :
			(Type == EStructNodeType::Column) ? 0.00010f : 0.00006f;
		Node.SelfWeight = Volume * Density;

		const float Strength = (Type == EStructNodeType::Column) ? 3.0f :
			(Type == EStructNodeType::Wall) ? 2.0f : 1.0f;
		Node.Capacity = FMath::Max(1.f, Volume * Strength * 0.000001f);
	}

	Nodes.Add(Node);
	return Node.Id;
}

void AStructGraphManager::BuildNodes()
{
	auto AddArray = [&](EStructNodeType Type, const TArray<TObjectPtr<UStaticMeshComponent>>& Arr, int32 FloorIdx)
		{
			for (UStaticMeshComponent* C : Arr)
			{
				if (!IsValid(C)) continue;
				AddNode(Type, C, FloorIdx);
			}
		};

	AddArray(EStructNodeType::Slab, SlabL1, 1);
	AddArray(EStructNodeType::Slab, SlabL2, 2);
	AddArray(EStructNodeType::Slab, SlabL3, 3);
	AddArray(EStructNodeType::Slab, SlabRoof, 99);

	AddArray(EStructNodeType::Column, Columns, -1);
	AddArray(EStructNodeType::Wall, Walls, -1);
}

void AStructGraphManager::SaveOriginalTransforms()
{
	for (FStructGraphNode& N : Nodes)
	{
		if (!N.Comp) continue;
		OriginalTransforms.Add(N.Comp, N.Comp->GetComponentTransform());
	}
}

FBox AStructGraphManager::GetCompBox(UPrimitiveComponent* Comp) const
{
	if (!Comp) return FBox(EForceInit::ForceInit);

	const FBoxSphereBounds B = Comp->Bounds;
	return B.GetBox();
}

bool AStructGraphManager::Overlap2D(const FBox& A, const FBox& B, float Expand)
{
	const FBox EA(FVector(A.Min.X - Expand, A.Min.Y - Expand, A.Min.Z),
		FVector(A.Max.X + Expand, A.Max.Y + Expand, A.Max.Z));
	const FBox EB(FVector(B.Min.X - Expand, B.Min.Y - Expand, B.Min.Z),
		FVector(B.Max.X + Expand, B.Max.Y + Expand, B.Max.Z));

	const bool bX = (EA.Min.X <= EB.Max.X) && (EA.Max.X >= EB.Min.X);
	const bool bY = (EA.Min.Y <= EB.Max.Y) && (EA.Max.Y >= EB.Min.Y);
	return bX && bY;
}

float AStructGraphManager::OverlapArea2D(const FBox& A, const FBox& B, float Expand)
{
	const float Ax0 = A.Min.X - Expand, Ax1 = A.Max.X + Expand;
	const float Ay0 = A.Min.Y - Expand, Ay1 = A.Max.Y + Expand;
	const float Bx0 = B.Min.X - Expand, Bx1 = B.Max.X + Expand;
	const float By0 = B.Min.Y - Expand, By1 = B.Max.Y + Expand;

	const float Ox = FMath::Max(0.f, FMath::Min(Ax1, Bx1) - FMath::Max(Ax0, Bx0));
	const float Oy = FMath::Max(0.f, FMath::Min(Ay1, By1) - FMath::Max(Ay0, By0));
	return Ox * Oy;
}


// BuildSupportGraph
void AStructGraphManager::BuildSupportGraph()
{
	// Id->Index 
	TMap<int32, int32> IdToIndex;
	IdToIndex.Reserve(Nodes.Num());
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		IdToIndex.Add(Nodes[i].Id, i);
	}

	// Supports 
	for (FStructGraphNode& N : Nodes)
	{
		if (N.Type == EStructNodeType::Slab)
		{
			N.Supports.Reset();
			N.SupportWeights.Reset();
		}
	}

	TArray<int32> SupporterIds;
	SupporterIds.Reserve(Nodes.Num());
	for (const FStructGraphNode& N : Nodes)
	{
		if (N.Type == EStructNodeType::Column || N.Type == EStructNodeType::Wall)
		{
			SupporterIds.Add(N.Id);
		}
	}

	for (FStructGraphNode& Slab : Nodes)
	{
		if (Slab.Type != EStructNodeType::Slab || !Slab.Comp)
			continue;

		const FBox SlabBox = GetCompBox(Slab.Comp);
		const FVector SlabCenter = SlabBox.GetCenter();
		const float SlabBottomZ = SlabBox.Min.Z;
		const int32 SlabFloor = Slab.FloorIndex;

		for (int32 Sid : SupporterIds)
		{
			const int32* SupIdx = IdToIndex.Find(Sid);
			if (!SupIdx) continue;

			FStructGraphNode& Sup = Nodes[*SupIdx];
			if (!Sup.Comp) continue;


			if (Sup.FloorIndex != -1)
			{
				if (!(Sup.FloorIndex == SlabFloor || Sup.FloorIndex == SlabFloor - 1))
					continue;
			}

			const FBox SupBox = GetCompBox(Sup.Comp);

			const float Z = SlabBottomZ;
			const bool bZHit =
				(Z >= SupBox.Min.Z - SupportZTolerance) &&
				(Z <= SupBox.Max.Z + SupportZTolerance);

			if (!bZHit)
				continue;

			if (!Overlap2D(SlabBox, SupBox, SupportXYExpand))
				continue;

			const float Area = OverlapArea2D(SlabBox, SupBox, SupportXYExpand);
			if (Area <= KINDA_SMALL_NUMBER)
				continue;

			const FVector SupCenter = SupBox.GetCenter();
			const float Dist2D = FVector::Dist2D(SlabCenter, SupCenter);
			const float Wdist = 1.f / FMath::Pow(Dist2D + WeightDistEpsilon, WeightDistPower);

			const float Wtype = (Sup.Type == EStructNodeType::Wall) ? WallTypeFactor : ColumnTypeFactor;

			float Wangle = 1.f;
			if (Sup.Type == EStructNodeType::Column)
			{
				const float Dot = FMath::Clamp(
					FVector::DotProduct(FVector::UpVector, Sup.Comp->GetUpVector().GetSafeNormal()),
					0.f, 1.f);

				Wangle = FMath::Pow(Dot, AnglePower);
			}

			const float Weight = Area * Wdist * Wtype * Wangle;
			if (Weight > KINDA_SMALL_NUMBER)
			{
				Slab.Supports.Add(Sup.Id);
				Slab.SupportWeights.Add(Weight);
			}
		}

		if (SlabFloor == 1 && Slab.Supports.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Support] L1 Slab %d has 0 supports -> treat as GROUND-supported (MVP)"), Slab.Id);
		}

		float SumW = 0.f;
		for (float W : Slab.SupportWeights) SumW += W;

		if (SumW > KINDA_SMALL_NUMBER)
		{
			for (float& W : Slab.SupportWeights)
				W /= SumW;
		}

		UE_LOG(LogTemp, Log, TEXT("[Support] Slab %d Floor=%d Supports=%d SumW=%.3f BottomZ=%.1f"),
			Slab.Id, SlabFloor, Slab.Supports.Num(), SumW, SlabBottomZ);
	}
}


// BuildDownGraph
void AStructGraphManager::BuildDownGraph()
{
	for (FStructGraphNode& N : Nodes)
	{
		if (N.Type == EStructNodeType::Column || N.Type == EStructNodeType::Wall)
		{
			N.ClearDown();
		}
	}

	TArray<int32> SlabIds;
	SlabIds.Reserve(Nodes.Num());
	for (const FStructGraphNode& N : Nodes)
	{
		if (N.Type == EStructNodeType::Slab)
		{
			SlabIds.Add(N.Id);
		}
	}

	for (FStructGraphNode& Sup : Nodes)
	{
		if (!(Sup.Type == EStructNodeType::Column || Sup.Type == EStructNodeType::Wall))
			continue;
		if (!Sup.Comp)
			continue;

		const FBox SupBox = GetCompBox(Sup.Comp);
		const float SupBottomZ = GetBottomZ(SupBox);

		float SumW = 0.f;

		for (int32 SlabId : SlabIds)
		{
			FStructGraphNode& Slab = Nodes[SlabId];
			if (!Slab.Comp) continue;

			const FBox SlabBox = GetCompBox(Slab.Comp);
			const float SlabTopZ = GetTopZ(SlabBox);

			if (FMath::Abs(SupBottomZ - SlabTopZ) > SupportZTolerance)
				continue;

			if (!Overlap2D(SupBox, SlabBox, SupportXYExpand))
				continue;

			const float Area = OverlapArea2D(SupBox, SlabBox, SupportXYExpand);
			if (Area <= KINDA_SMALL_NUMBER)
				continue;

			const float Wtype = (Sup.Type == EStructNodeType::Wall) ? WallTypeFactor : ColumnTypeFactor;

			const float W = Area * Wtype;
			Sup.DownSlabs.Add(Slab.Id);
			Sup.DownWeights.Add(W);
			SumW += W;
		}

		if (SumW > KINDA_SMALL_NUMBER)
		{
			for (float& W : Sup.DownWeights)
			{
				W /= SumW;
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[Down] Sup %d Type=%d DownSlabs=%d SumW=%.3f BottomZ=%.1f"),
			Sup.Id, (int32)Sup.Type, Sup.DownSlabs.Num(), SumW, SupBottomZ);
	}
}

bool AStructGraphManager::IsConnectedToGround_BFS(int32 StartNodeId, TMap<int32, bool>& Cache) const
{
	if (!Nodes.IsValidIndex(StartNodeId))
	{
		Cache.Add(StartNodeId, false);
		return false;
	}

	if (const bool* Found = Cache.Find(StartNodeId))
	{
		return *Found;
	}

	TSet<int32> Visited;
	TQueue<int32> Q;

	Q.Enqueue(StartNodeId);
	Visited.Add(StartNodeId);

	while (!Q.IsEmpty())
	{
		int32 CurId;
		Q.Dequeue(CurId);

		if (!Nodes.IsValidIndex(CurId))
			continue;

		const FStructGraphNode& Cur = Nodes[CurId];

		if (Cur.State == EStructDamageState::Failed)
			continue;

		if (Cur.Type == EStructNodeType::Slab && Cur.FloorIndex == 1)
		{
			Cache.Add(StartNodeId, true);
			return true;
		}

		// Slab -> Supports
		if (Cur.Type == EStructNodeType::Slab)
		{
			for (int32 SupId : Cur.Supports)
			{
				if (!Visited.Contains(SupId))
				{
					Visited.Add(SupId);
					Q.Enqueue(SupId);
				}
			}
		}
		// Supporter -> DownSlabs
		else if (Cur.Type == EStructNodeType::Column || Cur.Type == EStructNodeType::Wall)
		{
			for (int32 SlabId : Cur.DownSlabs)
			{
				if (!Visited.Contains(SlabId))
				{
					Visited.Add(SlabId);
					Q.Enqueue(SlabId);
				}
			}
		}
	}

	Cache.Add(StartNodeId, false);
	return false;
}


bool AStructGraphManager::UpdateDamageStates(bool& bAnyNewFailures)
{
	bAnyNewFailures = false;

	for (FStructGraphNode& N : Nodes)
	{
		if (!N.Comp) continue;
		if (N.State == EStructDamageState::Failed) continue;

		const float U = N.Utilization();

		if (N.Type == EStructNodeType::Slab)
		{
			if (U >= YieldStart)
			{
				if (N.State != EStructDamageState::Yield)
				{
					N.State = EStructDamageState::Yield;
					OnNodeYield(N.Comp, U);
				}
			}
			else
			{
				N.State = EStructDamageState::Intact;
			}
			continue;
		}

		if (U >= FailStart)
		{
			N.State = EStructDamageState::Failed;
			bAnyNewFailures = true;

			if (UPrimitiveComponent* PC = N.Comp)
			{
				PC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				PC->SetSimulatePhysics(true);
				PC->SetEnableGravity(true);
				PC->WakeAllRigidBodies();
			}

			OnNodeFailed(N.Comp, U);

		}
		else if (U >= YieldStart)
		{
			if (N.State != EStructDamageState::Yield)
			{
				N.State = EStructDamageState::Yield;
				OnNodeYield(N.Comp, U);
			}
		}
		else
		{
			N.State = EStructDamageState::Intact;
		}
	}
	return true;
}

void AStructGraphManager::SolveLoadsAndDamage()
{
	for (int32 Iter = 0; Iter < MaxSolveIterations; ++Iter)
	{

		const float Seismic = 0.f;

		ComputeLoadsFromScratch(Seismic);

		bool bNewFailures = false;
		UpdateDamageStates(bNewFailures);

		if (!bNewFailures) break;
	}

	if (bDrawDebug)
	{
		DrawDebugGraph();
	}
}

void AStructGraphManager::DrawDebugGraph()
{
	if (!GetWorld()) return;

	for (const FStructGraphNode& Slab : Nodes)
	{
		if (Slab.Type != EStructNodeType::Slab || !Slab.Comp) continue;

		const FVector A = Slab.Comp->GetComponentLocation();

		for (int32 SupId : Slab.Supports)
		{
			const FStructGraphNode& Sup = Nodes[SupId];
			if (!Sup.Comp) continue;

			const FVector B = Sup.Comp->GetComponentLocation();

			FColor Col = FColor::Green;
			if (Sup.State == EStructDamageState::Yield) Col = FColor::Yellow;
			if (Sup.State == EStructDamageState::Failed) Col = FColor::Red;

			DrawDebugLine(GetWorld(), A, B, Col, false, DebugLineDuration, 0, 2.f);
		}
	}
}

void AStructGraphManager::TickEarthquake()
{
	if (QuakeStage == EQuakeStage::Stage3)
	{
		return;
	}

	Elapsed += 0.05f;

	const float T1 = Phase1_Duration;
	const float T2 = Phase1_Duration + Phase2_Duration;
	const float T3 = Phase1_Duration + Phase2_Duration + Phase3_Duration;

	if (Elapsed < T1) QuakePhase = EQuakePhase::Micro;
	else if (Elapsed < T2) QuakePhase = EQuakePhase::Progress;
	else if (Elapsed < T3) QuakePhase = EQuakePhase::Collapse;
	else
	{
		StopEarthquake3Phase();
		return;
	}

	float LocalBase = 0.0f;
	float LocalOmega = SeismicOmega;

	switch (QuakePhase)
	{
	case EQuakePhase::Micro:
		LocalBase = FMath::Lerp(0.05f, 0.20f, Elapsed / FMath::Max(0.01f, T1));
		LocalOmega = SeismicOmega * 0.7f;
		break;

	case EQuakePhase::Progress:
		LocalBase = 0.35f;
		LocalOmega = SeismicOmega * 1.0f;
		break;

	case EQuakePhase::Collapse:
		LocalBase = 0.55f;
		LocalOmega = SeismicOmega * 1.2f;
		break;

	default:
		break;
	}

	const float Wave = FMath::Sin(Elapsed * LocalOmega);
	const float SeismicFactor = LocalBase * Wave;

	for (int32 Iter = 0; Iter < MaxSolveIterations; ++Iter)
	{
		ComputeLoadsFromScratch(SeismicFactor);

		bool bNewFailures = false;
		UpdateDamageStates(bNewFailures);

		if (!bNewFailures) break;
	}

	if (bDrawDebug)
	{
		DrawDebugGraph();
	}
}

void AStructGraphManager::OnNodeYield_Implementation(UPrimitiveComponent* Comp, float Utilization)
{
	AActor* ActorOwner = Comp ? Comp->GetOwner() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("[StructGraph] Yield: Actor=%s Comp=%s U=%.2f"),
		ActorOwner ? *ActorOwner->GetName() : TEXT("null"),
		Comp ? *Comp->GetName() : TEXT("null"),
		Utilization);
}

void AStructGraphManager::OnNodeFailed_Implementation(UPrimitiveComponent* Comp, float Utilization)
{
	AActor* ActorOwner = Comp ? Comp->GetOwner() : nullptr;
	UE_LOG(LogTemp, Warning, TEXT("[StructGraph] Failed: Actor=%s Comp=%s U=%.2f"),
		ActorOwner ? *ActorOwner->GetName() : TEXT("null"),
		Comp ? *Comp->GetName() : TEXT("null"),
		Utilization);
}

void AStructGraphManager::StabilizeStructureComponent(UPrimitiveComponent* PC)
{
	if (!PC) return;

	UE_LOG(LogTemp, Warning, TEXT("[CALL] StabilizeStructureComponent -> %s"), *GetNameSafe(PC ? PC->GetOwner() : nullptr));


	PC->SetSimulatePhysics(false);
	PC->SetEnableGravity(false);

	PC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	PC->SetLinearDamping(2.0f);
	PC->SetAngularDamping(2.0f);
}


void AStructGraphManager::ComputeLoadsFromScratch(float SeismicFactor)
{
	for (FStructGraphNode& N : Nodes)
	{
		N.CarriedLoad = 0.f;
	}

	TMap<int32, bool> GroundCache;

	for (FStructGraphNode& Slab : Nodes)
	{
		if (Slab.Type != EStructNodeType::Slab) continue;
		if (!Slab.Comp) continue;
		if (Slab.State == EStructDamageState::Failed) continue;

		if (Slab.FloorIndex == 1) continue;

		const float SlabLoad = Slab.SelfWeight; 

		float SumW = 0.f;
		TArray<int32> ValidIdx;
		ValidIdx.Reserve(Slab.Supports.Num());

		for (int32 i = 0; i < Slab.Supports.Num(); ++i)
		{
			if (!Slab.SupportWeights.IsValidIndex(i)) continue;

			const int32 SupId = Slab.Supports[i];
			if (!Nodes.IsValidIndex(SupId)) continue;

			const FStructGraphNode& Sup = Nodes[SupId];
			if (Sup.State == EStructDamageState::Failed) continue;

			if (!IsConnectedToGround_BFS(SupId, GroundCache)) continue;

			ValidIdx.Add(i);
			SumW += Slab.SupportWeights[i];
		}

		if (ValidIdx.Num() == 0 || SumW <= KINDA_SMALL_NUMBER)
			continue;

		for (int32 i : ValidIdx)
		{
			const int32 SupId = Slab.Supports[i];
			const float W = Slab.SupportWeights[i] / SumW;
			Nodes[SupId].CarriedLoad += SlabLoad * W;
		}
	}

	for (FStructGraphNode& Sup : Nodes)
	{
		if (!(Sup.Type == EStructNodeType::Column || Sup.Type == EStructNodeType::Wall)) continue;
		if (!Sup.Comp) continue;
		if (Sup.State == EStructDamageState::Failed) continue;

		const float Total = Sup.SelfWeight + Sup.CarriedLoad;

		if (Sup.DownSlabs.Num() == 0)
			continue;

		float SumW = 0.f;
		for (float W : Sup.DownWeights) SumW += W;
		if (SumW <= KINDA_SMALL_NUMBER) continue;

		for (int32 i = 0; i < Sup.DownSlabs.Num(); ++i)
		{
			if (!Sup.DownWeights.IsValidIndex(i)) continue;

			const int32 DownSlabId = Sup.DownSlabs[i];
			if (!Nodes.IsValidIndex(DownSlabId)) continue;

			if (Nodes[DownSlabId].State == EStructDamageState::Failed) continue;

			const float W = Sup.DownWeights[i] / SumW;
			Nodes[DownSlabId].CarriedLoad += Total * W;
		}
	}

	if (FMath::Abs(SeismicFactor) > KINDA_SMALL_NUMBER)
	{
		const float Seis = FMath::Abs(SeismicFactor);
		for (FStructGraphNode& N : Nodes)
		{
			const float Amp = 1.f + SeismicFloorAmplify * FMath::Max(0, N.FloorIndex - 1);
			N.CarriedLoad *= (1.f + Seis * SeismicToVerticalScale * Amp);
		}
	}
}


void AStructGraphManager::CalibrateCapacitiesFromInitialState()
{
	ComputeLoadsFromScratch(0.f);

	double SumU = 0.0;
	int32 Count = 0;

	for (FStructGraphNode& N : Nodes)
	{
		if (!(N.Type == EStructNodeType::Column || N.Type == EStructNodeType::Wall)) continue;
		if (!N.Comp) continue;

		const float U = N.Utilization();
		if (!FMath::IsFinite(U) || U <= 0.f) continue;

		SumU += U;
		Count++;
	}

	if (Count == 0) return;

	const float AvgU = (float)(SumU / (double)Count);

	const float TargetU = FMath::Max(0.05f, TargetInitialUtilization);
	const float Scale = (AvgU / TargetU) * CapacitySafetyFactor;

	if (!FMath::IsFinite(Scale) || Scale <= 0.f) return;

	for (FStructGraphNode& N : Nodes)
	{
		if (!(N.Type == EStructNodeType::Column || N.Type == EStructNodeType::Wall)) continue;
		N.Capacity *= Scale;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Calib] AvgU=%.3f Target=%.3f Scale=%.3f (CapacitySafetyFactor=%.2f)"),
		AvgU, TargetU, Scale, CapacitySafetyFactor);
}

void AStructGraphManager::CacheBaseCapacities()
{
	for (FStructGraphNode& N : Nodes)
	{
		N.BaseCapacity = FMath::Max(0.01f, N.Capacity);
		N.Damage = 0.f;
	}
}

void AStructGraphManager::ApplySeismicAndShearDemands(float SeismicFactor)
{
	const float Seis = FMath::Abs(SeismicFactor);
	if (Seis <= KINDA_SMALL_NUMBER) return;

	for (FStructGraphNode& N : Nodes)
	{
		if (!N.Comp) continue;
		if (N.State == EStructDamageState::Failed) continue;

		const int32 Floor = N.FloorIndex;

		const float Amp = 1.f + SeismicFloorAmplify * FMath::Max(0, Floor - 1);

		float Soft = 1.f;
		if (Floor == SoftStoryFloor)
		{
			Soft = SoftStoryBoost;
		}

		const float DemandBase = (N.SelfWeight + N.CarriedLoad);
		const float Added = DemandBase * Seis * ShearToVerticalScale * Amp * Soft;

		N.CarriedLoad += Added;
	}
}

void AStructGraphManager::AccumulateDamage(float DeltaTime)
{
	for (FStructGraphNode& N : Nodes)
	{
		if (!N.Comp) continue;
		if (N.State == EStructDamageState::Failed) continue;

		const float U = N.Utilization();

		if (U >= YieldStart)
		{
			float Rate = DamageRateYield;
			if (U >= FailStart) Rate = DamageRateFailed;

			const float UFactor = FMath::Clamp((U - YieldStart) / (FailStart - YieldStart + 1e-3f), 0.f, 2.f);

			N.Damage = FMath::Clamp(N.Damage + Rate * (0.5f + UFactor) * DeltaTime, 0.f, 1.f);

			const float Loss = CapacityLossAtFullDamage * N.Damage; // 0~CapacityLossAtFullDamage
			N.Capacity = FMath::Max(0.01f, N.BaseCapacity * (1.f - Loss));
		}
		else
		{
			//
		}
	}
}



void AStructGraphManager::StartEarthquake3Phase()
{
	if (bRunning) return;
	bRunning = true;
	Elapsed = 0.f;
	QuakePhase = EQuakePhase::Micro;

	GetWorldTimerManager().SetTimer(
		TickHandle,
		this,
		&AStructGraphManager::TickEarthquake,
		0.05f,
		true
	);
}

void AStructGraphManager::StopEarthquake3Phase()
{
	GetWorldTimerManager().ClearTimer(Stage3ShakeTimer);
	GetWorldTimerManager().ClearTimer(ImpactDecayTimer);
	StopEarthquake();
	QuakePhase = EQuakePhase::Idle;
}









void AStructGraphManager::SetQuakeStage(EQuakeStage NewStage)
{
	QuakeStage = NewStage;

	switch (QuakeStage)
	{
	case EQuakeStage::Stage1: TriggerStage1(); break;
	case EQuakeStage::Stage2: TriggerStage2(); break;
	case EQuakeStage::Stage3: TriggerStage3(); break;
	default: break;
	}
}

void AStructGraphManager::TriggerStage1()
{
	UE_LOG(LogTemp, Warning, TEXT("[Quake] Stage1 Start"));

	ApplyDampingToTaggedGC(GCWallTag, Stage1LinearDamping, Stage1AngularDamping);

	SeismicBase = Stage1_SeismicBase;
	SeismicOmega = Stage1_Omega;
	StartEarthquake(); 
	PlayShake(QuakeContinuousShakeClass, Stage1ShakeScale);
}

void AStructGraphManager::TriggerStage2()
{
	UE_LOG(LogTemp, Warning, TEXT("[Quake] Stage2 Start (Local strain pulses on GC walls)"));

	Stage2Stream.Initialize(Stage2Seed);

	Stage2_PulseCount = 0;
	Stage2_SpallBudget = 2;
	Stage2_ColumnShearIdx = 0;

	//EnablePhysicsForTaggedGC(GCWallTag, true, true);
	//EnablePhysicsForGCArray(GCWalls, true, true);
	/*EnablePhysicsForGCArray(GCColumns, true, true);
	EnablePhysicsForGCArray(GCSlabs, true, true);*/

	ApplyDampingToTaggedGC(GCWallTag, Stage2LinearDamping, Stage2AngularDamping);


	//SeismicBase = Stage1_SeismicBase;
	//SeismicOmega = Stage1_Omega;
	//StartEarthquake();

	Stage2Elapsed = 0.f;
	GetWorldTimerManager().ClearTimer(Stage2Timer);

	GetWorldTimerManager().SetTimer(
		Stage2Timer,
		this,
		&AStructGraphManager::TriggerPulse2,
		Stage2_PulseInterval,
		true
	);

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastStage2ShakeTime >= 0.5f) 
	{
		PlayShake(QuakeContinuousShakeClass, Stage2ShakeScale);
		LastStage2ShakeTime = Now;
	}
}

void AStructGraphManager::TriggerStage3()
{
	BuildGCCache();
	DumpGCCache(TEXT("BeforeStage3"));
	// 타이머 정리
	GetWorldTimerManager().ClearTimer(Stage2Timer);
	GetWorldTimerManager().ClearTimer(Stage3SlabDelayHandle);
	GetWorldTimerManager().ClearTimer(Stage3ContinuousHandle);

	StopEarthquake();

	UE_LOG(LogTemp, Warning, TEXT("[Quake] Stage3 Start (Single-flow continuous)"));

	PlayShake(QuakeStage3LongShakeClass, Stage3LongScale);
	DisableAllProxies();
	EnsureGCPhysicsReady_Stage3();
	StartStage3Continuous();
}

UGeometryCollectionComponent* AStructGraphManager::FindNearestGC(const FVector& WorldPoint) const
{
	UWorld* W = GetWorld();
	if (!W) return nullptr;

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("GC_WALL"), Found);

	float BestDistSq = TNumericLimits<float>::Max();
	UGeometryCollectionComponent* BestGC = nullptr;

	for (AActor* A : Found)
	{
		if (!IsValid(A)) continue;
		auto* GC = A->FindComponentByClass<UGeometryCollectionComponent>();
		if (!IsValid(GC)) continue;

		const float D2 = FVector::DistSquared(WorldPoint, A->GetActorLocation());
		if (D2 < BestDistSq)
		{
			BestDistSq = D2;
			BestGC = GC;
		}
	}
	return BestGC;
}



void AStructGraphManager::TriggerPulse2()
{
	Stage2Elapsed += Stage2_PulseInterval;

	if (Stage2Elapsed >= Stage2_Duration)
	{
		GetWorldTimerManager().ClearTimer(Stage2Timer);
		UE_LOG(LogTemp, Warning, TEXT("[Quake] Stage2 End"));
		return;
	}

	Stage2_PulseCount++;


	auto ForceStage2StageSimNoGrav = [](UGeometryCollectionComponent* GC)
		{
			if (!IsValid(GC)) return;

			GC->SetSimulatePhysics(true);   
			GC->SetEnableGravity(false);    

			GC->SetLinearDamping(8.0f);
			GC->SetAngularDamping(25.0f);

			GC->WakeAllRigidBodies();
		};

	// 1) WALL: crack (weak)
	UGeometryCollectionComponent* WallGC = nullptr;
	if (GCWalls.Num() > 0)
	{
		WallGC = PickRandomValidGC(GCWalls, Stage2Stream);
		if (IsValid(WallGC) && WallGC->IsRegistered() && !WallGC->IsBeingDestroyed())
		{
			ForceStage2StageSimNoGrav(WallGC);

			AActor* A = WallGC->GetOwner();
			if (IsValid(A) && !A->IsActorBeingDestroyed())
			{
				FVector Origin, Extent;
				A->GetActorBounds(true, Origin, Extent);

				const FVector Base = Origin - FVector(0, 0, Extent.Z * 0.75f);

				const FVector CrackPoint = Base + FVector(
					Stage2Stream.FRandRange(-Extent.X * 0.30f, Extent.X * 0.30f),
					Stage2Stream.FRandRange(-Extent.Y * 0.15f, Extent.Y * 0.15f),
					Stage2Stream.FRandRange(-8.f, 8.f)
				);

				const float CrackRadius = 120.f;
				const float CrackMag = 900.f;    
				ApplyStrainToGC(WallGC, CrackPoint, CrackRadius, CrackMag, 1);
			}
		}
	}

	// 2) COLUMN: crack (weak, shear-like)
	UGeometryCollectionComponent* ColGC = nullptr;
	if (GCColumns.Num() > 0)
	{
		ColGC = PickRandomValidGC(GCColumns, Stage2Stream);
		if (IsValid(ColGC) && ColGC->IsRegistered() && !ColGC->IsBeingDestroyed())
		{
			ForceStage2StageSimNoGrav(ColGC);

			AActor* A = ColGC->GetOwner();
			if (IsValid(A) && !A->IsActorBeingDestroyed())
			{
				FVector Origin, Extent;
				A->GetActorBounds(true, Origin, Extent);

				const FVector Base = Origin - FVector(0, 0, Extent.Z * 0.75f);

				// 하부 4방향 -> 전단
				Stage2_ColumnShearIdx = (Stage2_ColumnShearIdx + 1) % 4;

				FVector Offset = FVector::ZeroVector;
				switch (Stage2_ColumnShearIdx)
				{
				case 0: Offset = FVector(18.f, 0.f, 0.f); break;
				case 1: Offset = FVector(-18.f, 0.f, 0.f); break;
				case 2: Offset = FVector(0.f, 18.f, 0.f); break;
				default:Offset = FVector(0.f, -18.f, 0.f); break;
				}

				const FVector CrackPoint = Base + Offset + FVector(
					Stage2Stream.FRandRange(-6.f, 6.f),
					Stage2Stream.FRandRange(-6.f, 6.f),
					Stage2Stream.FRandRange(-6.f, 6.f)
				);

				const float CrackRadius = 70.f; 
				const float CrackMag = 1100.f;  
				ApplyStrainToGC(ColGC, CrackPoint, CrackRadius, CrackMag, 1);
			}
		}
	}

	// Stage2 끝날 때쯤(후반부)에만 한번씩 "한 조각" 떨어지게 유도
	if (Stage2_SpallBudget > 0)
	{
		const float Alpha = Stage2Elapsed / FMath::Max(0.01f, Stage2_Duration);
		const bool bLate = (Alpha > 0.65f);

		// 랜덤하게 가끔만 실행
		const bool bDoSpall = bLate && (Stage2Stream.FRand() < 0.25f);

		if (bDoSpall)
		{
			Stage2_SpallBudget--;

			UGeometryCollectionComponent* Target = nullptr;

			// 벽/기둥 중 하나 선택
			if (IsValid(WallGC) && IsValid(ColGC))
			{
				Target = (Stage2Stream.FRand() < 0.6f) ? WallGC : ColGC;
			}
			else
			{
				Target = IsValid(WallGC) ? WallGC : ColGC;
			}

			if (IsValid(Target))
			{
				AActor* A = Target->GetOwner();
				if (IsValid(A))
				{
					FVector Origin, Extent;
					A->GetActorBounds(true, Origin, Extent);
					const FVector Base = Origin - FVector(0, 0, Extent.Z * 0.75f);

					const FVector SpallPoint = Base + FVector(
						Stage2Stream.FRandRange(-25.f, 25.f),
						Stage2Stream.FRandRange(-25.f, 25.f),
						Stage2Stream.FRandRange(-5.f, 5.f)
					);

					const float SpallRadius = 30.f;    
					const float SpallMag = 18000.f;    
					ApplyStrainToGC(Target, SpallPoint, SpallRadius, SpallMag, 1);

					UE_LOG(LogTemp, Warning, TEXT("[Stage2] Spall (%d left) on %s"),
						Stage2_SpallBudget, *GetNameSafe(A));
				}
			}
		}
	}

	// 카메라 흔들림(연출만)
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastStage2ShakeTime >= 0.5f)
	{
		PlayShake(QuakeContinuousShakeClass, Stage2ShakeScale);
		LastStage2ShakeTime = Now;
	}
}


void AStructGraphManager::ApplyStrainToGC(
	UGeometryCollectionComponent* GCComp,
	const FVector& HitPoint,
	float Radius,
	float StrainMagnitude,
	int32 Iterations)
{
	if (!IsValid(GCComp)) return;

	URadialFalloff* Falloff = NewObject<URadialFalloff>(GCComp);
	if (!Falloff) return;

	//GCComp->SetSimulatePhysics(true);
	//GCComp->SetEnableGravity(true);
	GCComp->WakeAllRigidBodies();

	Falloff->SetRadialFalloff(
		StrainMagnitude,
		0.0f,
		1.0f,
		0.0f,
		Radius,
		HitPoint,
		EFieldFalloffType::Field_FallOff_None
	);

	UFieldSystemMetaDataIteration* Iter = NewObject<UFieldSystemMetaDataIteration>(GCComp);
	if (Iter) Iter->Iterations = FMath::Clamp(Iterations, 1, 10);

	GCComp->ApplyPhysicsField(
		true,
		EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain,
		Iter,
		Falloff
	);
}

void AStructGraphManager::Debug_ApplyStrainOnce()
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("GC_WALL"), Found);

	UE_LOG(LogTemp, Warning, TEXT("[Debug] GC_WALL Found: %d"), Found.Num());
	if (Found.Num() == 0) return;

	AActor* WallActor = Found[0];
	if (!IsValid(WallActor)) return;

	UGeometryCollectionComponent* GCComp = WallActor->FindComponentByClass<UGeometryCollectionComponent>();
	if (!IsValid(GCComp))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Debug] No GCComp on %s"), *WallActor->GetName());
		return;
	}

	FVector Origin, Extent;
	WallActor->GetActorBounds(true, Origin, Extent);

	const float Radius = 2000.f;
	const float Mag = 1000000.f;

	UE_LOG(LogTemp, Warning, TEXT("[Debug] ApplyStrainOnce -> %s Radius=%.1f Mag=%.1f"),
		*WallActor->GetName(), Radius, Mag);

	ApplyStrainToGC(GCComp, Origin, Radius, Mag, 10);

	GCComp->SetSimulatePhysics(true);
	UE_LOG(LogTemp, Warning, TEXT("[GCPhys] Sim=%d Mobility=%d Collision=%d"),
		GCComp->IsSimulatingPhysics() ? 1 : 0,
		(int32)GCComp->Mobility,
		(int32)GCComp->GetCollisionEnabled());
	GCComp->SetEnableGravity(true);
	GCComp->WakeAllRigidBodies();

	GCComp->AddImpulse(FVector(20000000.f, 0.f, 0.f), NAME_None, true);
}




void AStructGraphManager::PlayStage1CameraShake()
{
	if (!Stage1_CameraShakeClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Stage1] Stage1_CameraShakeClass is null. Set it in BP_StructGraphManager."));
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager) return;

	PC->PlayerCameraManager->StartCameraShake(Stage1_CameraShakeClass, Stage1_ShakeScale);
}

void AStructGraphManager::DestroyActorsWithTag(FName Tag)
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, Found);

	UE_LOG(LogTemp, Warning, TEXT("[Stage1] DestroyActorsWithTag(%s) Found=%d"), *Tag.ToString(), Found.Num());

	for (AActor* A : Found)
	{
		if (!IsValid(A)) continue;
		if (A == this) continue;
		A->Destroy();
	}
}

void AStructGraphManager::ApplySettleDampingThenRestore()
{
	ApplyDampingToTaggedGC(GCWallTag, SettleLinearDamping, SettleAngularDamping);

	GetWorldTimerManager().ClearTimer(SettleTimerHandle);
	GetWorldTimerManager().SetTimer(SettleTimerHandle, [this]()
		{
			ApplyDampingToTaggedGC(GCWallTag, Stage1LinearDamping, Stage1AngularDamping);
		}, SettleDuration, false);
}

void AStructGraphManager::ApplyDampingToTaggedGC(FName Tag, float Lin, float Ang)
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, Found);

	for (AActor* A : Found)
	{
		if (!IsValid(A)) continue;

		UGeometryCollectionComponent* GC = A->FindComponentByClass<UGeometryCollectionComponent>();
		if (!IsValid(GC)) continue;

		GC->SetLinearDamping(Lin);
		GC->SetAngularDamping(Ang);
	}
}

void AStructGraphManager::PlayShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale)
{
	if (!ShakeClass) return;
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager) return;
	PC->PlayerCameraManager->StartCameraShake(ShakeClass, Scale);
}

void AStructGraphManager::PlayImpactDecaying()
{
	if (!QuakeImpactShakeClass) return;

	GetWorldTimerManager().ClearTimer(ImpactDecayTimer);

	const float StartTime = GetWorld()->GetTimeSeconds();
	ImpactDecayScale = ImpactStartScale;

	GetWorldTimerManager().SetTimer(ImpactDecayTimer, [this, StartTime]()
		{
			const float Now = GetWorld()->GetTimeSeconds();
			const float Alpha = FMath::Clamp((Now - StartTime) / FMath::Max(0.01f, ImpactDecayDuration), 0.f, 1.f);

			const float Scale = FMath::Lerp(ImpactStartScale, ImpactEndScale, Alpha);

			PlayShake(QuakeImpactShakeClass, Scale);

			if (Alpha >= 1.f)
			{
				GetWorldTimerManager().ClearTimer(ImpactDecayTimer);
			}

		}, ImpactDecayInterval, true);
}



// ===== Physics (tag) =====

void AStructGraphManager::EnablePhysicsForTaggedGC(FName Tag, bool bEnableSim, bool bEnableGrav)
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, Found);

	for (AActor* A : Found)
	{
		if (!IsValid(A) || A->IsActorBeingDestroyed()) continue;

		UGeometryCollectionComponent* GC = A->FindComponentByClass<UGeometryCollectionComponent>();
		if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed()) continue;

		GC->SetSimulatePhysics(bEnableSim);
		GC->SetEnableGravity(bEnableGrav);

		if (bEnableGrav && !GC->IsGravityEnabled())
		{
			GC->SetSimulatePhysics(false);
			GC->RecreatePhysicsState();
			GC->SetSimulatePhysics(true);
			GC->SetEnableGravity(true);
		}

		if (bEnableSim)
		{
			GC->WakeAllRigidBodies();
		}
	}
}

// ===== Cache =====

void AStructGraphManager::ClearGCCache()
{
	GCWalls.Reset();
	GCColumns.Reset();
	GCSlabs.Reset();
}

void AStructGraphManager::BuildGCCache()
{
	ClearGCCache();

	auto GatherByTag = [&](FName Tag, TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Out)
		{
			TArray<AActor*> Found;
			UGameplayStatics::GetAllActorsWithTag(GetWorld(), Tag, Found);

			for (AActor* A : Found)
			{
				if (!IsValid(A) || A->IsActorBeingDestroyed()) continue;

				UGeometryCollectionComponent* GC = A->FindComponentByClass<UGeometryCollectionComponent>();
				if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed()) continue;

				Out.Add(GC);
			}
		};

	GatherByTag(GCWallTag, GCWalls);
	GatherByTag(FName("GC_COLUMN"), GCColumns);
	GatherByTag(FName("GC_SLAB"), GCSlabs);

	UE_LOG(LogTemp, Warning, TEXT("[GCCache] Walls=%d Cols=%d Slabs=%d"),
		GCWalls.Num(), GCColumns.Num(), GCSlabs.Num());

	auto SafeZ = [&](const TWeakObjectPtr<UGeometryCollectionComponent>& P) -> float
		{
			UGeometryCollectionComponent* GC = P.Get();
			if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed()) return TNumericLimits<float>::Max();

			AActor* OA = GC->GetOwner();
			if (IsValid(OA)) return OA->GetActorLocation().Z;

			return GC->GetComponentLocation().Z;
		};

	auto SortByZSafe = [&](TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr)
		{
			Arr.Sort([&](const TWeakObjectPtr<UGeometryCollectionComponent>& A,
				const TWeakObjectPtr<UGeometryCollectionComponent>& B)
				{
					return SafeZ(A) < SafeZ(B);
				});
		};

	SortByZSafe(GCColumns);
	SortByZSafe(GCWalls);
	SortByZSafe(GCSlabs);

	auto BindBreak = [&](const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr)
		{
			for (const auto& W : Arr)
			{
				UGeometryCollectionComponent* GC = W.Get();
				if (!IsValid(GC)) continue;

				GC->SetNotifyBreaks(true);

				GC->OnChaosBreakEvent.RemoveDynamic(this, &AStructGraphManager::OnGCBreak);
				GC->OnChaosBreakEvent.AddDynamic(this, &AStructGraphManager::OnGCBreak);
			}
		};

	BindBreak(GCWalls);
	BindBreak(GCColumns);
	BindBreak(GCSlabs);

	bBoundBreakEvents = true;
}

void AStructGraphManager::ForEachValidGC(
	const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr,
	TFunctionRef<void(UGeometryCollectionComponent*)> Fn) const
{
	for (const TWeakObjectPtr<UGeometryCollectionComponent>& W : Arr)
	{
		UGeometryCollectionComponent* GC = W.Get();
		if (!IsValid(GC)) continue;
		Fn(GC);
	}
}

UGeometryCollectionComponent* AStructGraphManager::PickRandomValidGC(
	TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr,
	FRandomStream& Stream) const
{
	const int32 N = Arr.Num();
	if (N == 0) return nullptr;

	for (int32 Try = 0; Try < 8; ++Try)
	{
		const int32 Idx = Stream.RandRange(0, N - 1);
		UGeometryCollectionComponent* GC = Arr[Idx].Get();
		if (IsValid(GC) && GC->IsRegistered() && !GC->IsBeingDestroyed()) return GC;
	}
	return nullptr;
}

// ===== EnablePhysicsForGCArray =====

void AStructGraphManager::EnablePhysicsForGCArray(
	const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr,
	bool bEnableSim, bool bEnableGrav)
{
	ForEachValidGC(Arr, [&](UGeometryCollectionComponent* GC)
		{
			if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed()) return;

			if (GC->Mobility != EComponentMobility::Movable)
			{
				GC->SetMobility(EComponentMobility::Movable);
			}

			GC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			GC->SetCollisionProfileName(TEXT("PhysicsActor"));
			GC->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

			const bool bWasSim = GC->IsSimulatingPhysics();

			// 1) 처음 Sim 켤 때: 1회 Recreate
			if (!bWasSim && bEnableSim)
			{
				GC->SetSimulatePhysics(false);
				GC->RecreatePhysicsState();
			}

			// 2) 목표 상태 적용
			GC->SetSimulatePhysics(bEnableSim);
			GC->SetEnableGravity(bEnableGrav);

			// 3) BodyInstance 레벨 강제
			if (FBodyInstance* BI = GC->GetBodyInstance())
			{
				if (bEnableGrav) BI->SetEnableGravity(true);
				if (bEnableSim)  BI->WakeInstance();
			}
			if (bEnableSim && bEnableGrav)
			{
				const bool bNeedFix = (!GC->IsSimulatingPhysics()) || (!GC->IsGravityEnabled());
				if (bNeedFix)
				{
					GC->SetSimulatePhysics(false);
					GC->RecreatePhysicsState();
					GC->SetSimulatePhysics(true);
					GC->SetEnableGravity(true);

					if (FBodyInstance* BI2 = GC->GetBodyInstance())
					{
						BI2->SetEnableGravity(true);
						BI2->WakeInstance();
					}
				}
			}

			// 5) wake + small impulse
			if (bEnableSim)
			{
				GC->WakeAllRigidBodies();
				if (bEnableGrav)
				{
					GC->AddImpulse(FVector(0, 0, -2000.f), NAME_None, true);
				}
				GC->WakeAllRigidBodies();
			}

			UE_LOG(LogTemp, Warning, TEXT("[EnableGCArray] %s Sim=%d Grav=%d Awake=%d WasSim=%d"),
				*GetNameSafe(GC->GetOwner()),
				GC->IsSimulatingPhysics() ? 1 : 0,
				GC->IsGravityEnabled() ? 1 : 0,
				GC->IsAnyRigidBodyAwake() ? 1 : 0,
				bWasSim ? 1 : 0);
		});
}

// ===== Pick / Find =====

UGeometryCollectionComponent* AStructGraphManager::PickLowestColumnGC() const
{
	// 유효 컬럼 수집
	TArray<UGeometryCollectionComponent*> ValidCols;
	ValidCols.Reserve(GCColumns.Num());

	for (const auto& W : GCColumns)
	{
		UGeometryCollectionComponent* GC = W.Get();
		if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed()) continue;

		AActor* OA = GC->GetOwner();
		if (!IsValid(OA) || OA->IsActorBeingDestroyed()) continue;

		ValidCols.Add(GC);
	}

	if (ValidCols.Num() == 0) return nullptr;

	// "낮은 Z" 순으로 정렬
	ValidCols.Sort([](const UGeometryCollectionComponent& A, const UGeometryCollectionComponent& B)
		{
			const AActor* AO = A.GetOwner();
			const AActor* BO = B.GetOwner();
			const float AZ = AO ? AO->GetActorLocation().Z : A.GetComponentLocation().Z;
			const float BZ = BO ? BO->GetActorLocation().Z : B.GetComponentLocation().Z;
			return AZ < BZ;
		});

	// 최저층 후보 몇 개만 추림
	const int32 CandCount = FMath::Clamp(Stage3_LowColumnCandidateCount, 1, ValidCols.Num());

	// 건물 중심(컬럼 전체 평균) 계산
	FVector Center(0, 0, 0);
	for (UGeometryCollectionComponent* GC : ValidCols)
	{
		const AActor* OA = GC->GetOwner();
		Center += (OA ? OA->GetActorLocation() : GC->GetComponentLocation());
	}
	Center /= float(ValidCols.Num());

	// 후보 중 "중심에서 가장 먼" 기둥 선택 (외곽 우선)
	UGeometryCollectionComponent* Best = nullptr;
	float BestScore = -1.f;

	for (int32 i = 0; i < CandCount; ++i)
	{
		UGeometryCollectionComponent* GC = ValidCols[i];
		const AActor* OA = GC->GetOwner();
		const FVector P = (OA ? OA->GetActorLocation() : GC->GetComponentLocation());

		const float Dist2D = FVector::Dist2D(P, Center);

		// 외곽 선호 강도
		const float Score = FMath::Pow(Dist2D, FMath::Max(0.01f, Stage3_PerimeterPreferPower));

		if (Score > BestScore)
		{
			BestScore = Score;
			Best = GC;
		}
	}

	return Best ? Best : ValidCols[0];
}

UGeometryCollectionComponent* AStructGraphManager::FindNearestSlabGC(const FVector& WorldPoint) const
{
	UGeometryCollectionComponent* Best = nullptr;
	float BestD2 = TNumericLimits<float>::Max();

	for (const auto& W : GCSlabs)
	{
		UGeometryCollectionComponent* Slab = W.Get();
		if (!IsValid(Slab) || !Slab->IsRegistered() || Slab->IsBeingDestroyed()) continue;

		const FVector Center = Slab->Bounds.Origin;
		const float D2 = FVector::DistSquared(Center, WorldPoint);

		if (D2 < BestD2)
		{
			BestD2 = D2;
			Best = Slab;
		}
	}
	return Best;
}

void AStructGraphManager::OnGCBreak(const FChaosBreakEvent& BreakEvent)
{
	UGeometryCollectionComponent* GC = Cast<UGeometryCollectionComponent>(BreakEvent.Component);
	if (!IsValid(GC)) return;

	UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	
	// ----------------------------
	// 1) 깨짐 직후 물리/중력 꼬임 보정
	// ----------------------------
	GC->SetSimulatePhysics(true);
	if (bStage3_UseManualGravity)
	{
		GC->SetEnableGravity(false);
	}
	else
	{
		GC->SetEnableGravity(true);
	}

	if (FBodyInstance* BI = GC->GetBodyInstance())
	{
		BI->SetEnableGravity(true);
		BI->WakeInstance();
	}
	GC->WakeAllRigidBodies();

	// 다음 틱 1회 추가 보정 (proxy 갱신 타이밍 대응)
	TWeakObjectPtr<UGeometryCollectionComponent> WeakGC(GC);
	World->GetTimerManager().SetTimerForNextTick([WeakGC]()
		{
			UGeometryCollectionComponent* G = WeakGC.Get();
			if (!IsValid(G)) return;

			G->SetEnableGravity(true);
			if (FBodyInstance* BI2 = G->GetBodyInstance())
			{
				BI2->SetEnableGravity(true);
				BI2->WakeInstance();
			}
			G->WakeAllRigidBodies();
		});

	// 2) Dust/Smoke Niagara spawn
	if (!DustNiagara) return;

	// BreakEvent.Component가 nullptr일 수 있으니 안전하게
	UPrimitiveComponent* Comp = Cast<UPrimitiveComponent>(BreakEvent.Component.Get());
	if (!IsValid(Comp)) return;

	const float Now = World->GetTimeSeconds();

	float& Last = DustLastTime.FindOrAdd(Comp);
	if (Now - Last < DustCooldown)
	{
		return;
	}
	Last = Now;

	const float Speed = BreakEvent.Velocity.Size();
	if (Speed < 80.f) // 필요하면 50~200 사이 튜닝
	{
		return;
	}

	const FVector P = BreakEvent.Location;

	// 스케일: 속도 기반 (너무 크면 안개처럼 덮임)
	const float Strength = FMath::Clamp(Speed / 600.f, 0.6f, 2.0f);
	const FVector ScaleVec = FVector(DustScale * Strength);

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		DustNiagara,                 // NS_h_Smoke 넣으면 됨
		P,
		FRotator::ZeroRotator,
		ScaleVec,
		true,
		true,
		ENCPoolMethod::AutoRelease
	);
	AddGravityAssist(GC, Stage3_GravityAssistDuration);
}

void AStructGraphManager::EnsureGCPhysicsReady_Stage3()
{
	const bool bBuiltinGrav = !bStage3_UseManualGravity;
	EnablePhysicsForGCArray_NoRecreate(GCWalls, true, bBuiltinGrav);
	EnablePhysicsForGCArray_NoRecreate(GCColumns, true, bBuiltinGrav);

	EnablePhysicsForGCArray_NoRecreate(GCSlabs, true, false);

	// 슬래브 댐핑 크게
	ForEachValidGC(GCSlabs, [&](UGeometryCollectionComponent* GC)
		{
			GC->SetLinearDamping(Stage3_SlabHoldLinStart);
			GC->SetAngularDamping(Stage3_SlabHoldAngStart);
			GC->WakeAllRigidBodies();
		});

	ForEachValidGC(GCWalls, [](UGeometryCollectionComponent* GC)
		{
			GC->SetLinearDamping(1.5f);
			GC->SetAngularDamping(1.5f);
			GC->WakeAllRigidBodies();
		});

	ForEachValidGC(GCColumns, [](UGeometryCollectionComponent* GC)
		{
			GC->SetLinearDamping(1.0f);
			GC->SetAngularDamping(1.0f);
			GC->WakeAllRigidBodies();
		});

	UE_LOG(LogTemp, Warning, TEXT("[Stage3 Ready] Walls=%d Cols=%d Slabs=%d"),
		GCWalls.Num(), GCColumns.Num(), GCSlabs.Num());
}
void AStructGraphManager::ApplyContinuousShakeToGC(
	const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr,
	float ImpulseStrength)
{
	ForEachValidGC(Arr, [&](UGeometryCollectionComponent* GC)
		{
			if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed()) return;

			const float Sign = (Stage3Stream.FRand() < 0.5f) ? -1.f : 1.f;
			const FVector Dir(Sign, 0.f, 0.f); // X축만

			const FVector Imp = Dir * ImpulseStrength + FVector(0, 0, Stage3_ShakeUpImpulse);
			GC->AddImpulse(Imp, NAME_None, false);
			if (!GC->IsAnyRigidBodyAwake())
			{
				GC->WakeAllRigidBodies();
			}
		});
}

void AStructGraphManager::StartStage3Continuous()
{
	if (bStage3ContinuousRunning) return;

	Stage3_RecreatedOnce.Reset();

	bStage3ContinuousRunning = true;

	Stage3TickCounter = 0;
	Stage3StartTimeSec = GetWorld()->GetTimeSeconds();


	if (Stage3Stream.GetInitialSeed() == 0)
	{
		Stage3Stream.Initialize(Stage2Seed + 777);
	}

	bStage3GravityCommitted = false;

	if (!Stage3_WeakColumnGC.IsValid())
	{
		Stage3_WeakColumnGC = PickLowestColumnGC();
	}

	if (Stage3_TargetSlabPunchPoint.IsNearlyZero() && Stage3_WeakColumnGC.IsValid())
	{
		if (AActor* ColA = Stage3_WeakColumnGC->GetOwner())
		{
			FVector Origin, Extent;
			ColA->GetActorBounds(true, Origin, Extent);
			Stage3_TargetSlabPunchPoint = Origin + FVector(0, 0, Extent.Z * 0.45f);
		}
	}

	if (!Stage3_TargetSlabGC.IsValid() && Stage3_WeakColumnGC.IsValid())
	{
		if (AActor* ColA = Stage3_WeakColumnGC->GetOwner())
		{
			FVector Origin, Extent;
			ColA->GetActorBounds(true, Origin, Extent);
			Stage3_TargetSlabGC = FindNearestSlabGC(Origin);
		}
	}

	EnsureGCPhysicsReady_Stage3();

	GetWorldTimerManager().ClearTimer(Stage3ContinuousHandle);
	GetWorldTimerManager().SetTimer(
		Stage3ContinuousHandle,
		this,
		&AStructGraphManager::Stage3_ContinuousTick,
		Stage3_TickInterval,
		true
	);

	UE_LOG(LogTemp, Warning, TEXT("[Stage3] Continuous Start (Weak=%s Slab=%s Seed=%d)"),
		*GetNameSafe(Stage3_WeakColumnGC.Get() ? Stage3_WeakColumnGC->GetOwner() : nullptr),
		*GetNameSafe(Stage3_TargetSlabGC.Get() ? Stage3_TargetSlabGC->GetOwner() : nullptr),
		Stage3Stream.GetInitialSeed());
}

void AStructGraphManager::StopStage3Continuous()
{
	if (!bStage3ContinuousRunning) return;
	bStage3ContinuousRunning = false;

	GetWorldTimerManager().ClearTimer(Stage3ContinuousHandle);

	if (bStage3_UseManualGravity)
	{
		StartStage3ManualGravity(Stage3_ManualGravityPostDuration);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Stage3] Continuous End"));
}

void AStructGraphManager::Stage3_ContinuousTick()
{

	Stage3TickCounter++;

	if (bStage3_UseManualGravity)
	{
		ApplyManualGravityToGCArray(GCWalls, Stage3_ManualGravityScale);
		ApplyManualGravityToGCArray(GCColumns, Stage3_ManualGravityScale);
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float T = Now - Stage3StartTimeSec;

	// 종료
	if (T >= Stage3_TotalDuration)
	{
		StopStage3Continuous();
		return;
	}

	// 0) 슬래브 하부부터
	ApplySlabRampForce_BottomUp(T);

	if (bStage3_UseManualGravity && T >= Stage3_SlabRampEndTime)
	{
		ApplyManualGravityToGCArray(GCSlabs, Stage3_ManualGravityScale);
	}

	if (!bStage3GravityCommitted && T >= Stage3_SlabRampEndTime)
	{
		bStage3GravityCommitted = true;

		ForEachValidGC(GCSlabs, [](UGeometryCollectionComponent* GC)
			{
				if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed()) return;
				GC->SetSimulatePhysics(true);
				GC->SetEnableGravity(true);
				if (FBodyInstance* BI = GC->GetBodyInstance())
				{
					BI->SetEnableGravity(true);
					BI->WakeInstance();
				}
				GC->WakeAllRigidBodies();
			});

		UE_LOG(LogTemp, Warning, TEXT("[Stage3] Slab Gravity ON"));
	}

	// 1) 슬래브 홀드(댐핑) 점진 해제
	{
		const float Alpha = FMath::Clamp(T / FMath::Max(0.01f, Stage3_HoldEndTime), 0.f, 1.f);
		const float Lin = FMath::Lerp(Stage3_SlabHoldLinStart, Stage3_SlabHoldLinEnd, Alpha);
		const float Ang = FMath::Lerp(Stage3_SlabHoldAngStart, Stage3_SlabHoldAngEnd, Alpha);

		ForEachValidGC(GCSlabs, [&](UGeometryCollectionComponent* GC)
			{
				if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed()) return;
				GC->SetLinearDamping(Lin);
				GC->SetAngularDamping(Ang);
			});
	}

	// 2) 연속 흔들림(지진 외력)
	if (bStage3_UseManualGravity)
	{
		ApplyManualGravityToGCArray(GCWalls, Stage3_ManualGravityScale);
		ApplyManualGravityToGCArray(GCColumns, Stage3_ManualGravityScale);
	}
	if (bStage3GravityCommitted)
	{
		ApplyContinuousShakeToGC(GCWalls, Stage3_ShakeImpulse * 0.8f);
		ApplyContinuousShakeToGC(GCColumns, Stage3_ShakeImpulse * 1.0f);
		ApplyContinuousShakeToGC(GCSlabs, Stage3_ShakeImpulse * 0.25f);
	}

	TickGravityAssist();

	// 3) strain은 듬성듬성: 0.05 틱이면 %12 => 0.6초마다 1회
	const bool bDoStrain = (Stage3TickCounter % 12) == 0;
	if (!bDoStrain) return;

	const bool bPhaseA = (T < 2.0f);                // 전단 준비
	const bool bPhaseB = (T >= 2.0f && T < 4.0f);   // 펀치 누적
	const bool bPhaseC = (T >= 4.0f);               // 마무리(과한 분해 금지)



	// 4) 벽 하부 균열 유지 (대상 1개만, 아주 약하게)
	{
		const int32 HitCount = FMath::Min(1, GCWalls.Num());
		for (int32 i = 0; i < HitCount; ++i)
		{
			UGeometryCollectionComponent* WallGC = GCWalls[i].Get();
			if (!IsValid(WallGC) || !WallGC->IsRegistered() || WallGC->IsBeingDestroyed()) continue;

			AActor* A = WallGC->GetOwner();
			if (!IsValid(A) || A->IsActorBeingDestroyed()) continue;

			FVector Origin, Extent;
			A->GetActorBounds(true, Origin, Extent);

			const FVector Base = Origin - FVector(0, 0, Extent.Z * 0.7f);
			const FVector P = Base + FVector(
				Stage3Stream.FRandRange(-Extent.X * 0.2f, Extent.X * 0.2f),
				Stage3Stream.FRandRange(-Extent.Y * 0.2f, Extent.Y * 0.2f),
				Stage3Stream.FRandRange(-10.f, 10.f)
			);

			ApplyStrainToGC(WallGC, P, Stage3_BaseStrainRadius, Stage3_BaseStrainMag, 1);
		}
	}

	// 5) 전단 누적
	{
		UGeometryCollectionComponent* ColGC = Stage3_WeakColumnGC.Get();
		if (IsValid(ColGC) && ColGC->IsRegistered() && !ColGC->IsBeingDestroyed())
		{
			AActor* ColA = ColGC->GetOwner();
			if (IsValid(ColA) && !ColA->IsActorBeingDestroyed())
			{
				FVector Origin, Extent;
				ColA->GetActorBounds(true, Origin, Extent);

				const FVector Base = Origin - FVector(0, 0, Extent.Z * 0.7f);

				static int32 ShearIdx = 0;
				ShearIdx = (ShearIdx + 1) % 4;

				FVector Offset;
				switch (ShearIdx)
				{
				case 0: Offset = FVector(12.f, 0.f, 0.f); break;
				case 1: Offset = FVector(-12.f, 0.f, 0.f); break;
				case 2: Offset = FVector(0.f, 12.f, 0.f); break;
				default:Offset = FVector(0.f, -12.f, 0.f); break;
				}

				const float Mag =
					bPhaseA ? Stage3_WeakStrainMag :
					bPhaseB ? Stage3_WeakStrainMag * 0.75f :
					Stage3_WeakStrainMag * 0.55f;

				ApplyStrainToGC(ColGC, Base + Offset, Stage3_WeakStrainRadius, Mag, 1);
			}
		}
	}

	// 6) 슬래브 펀치 누적: PhaseB에서만
	if (bPhaseB)
	{
		UGeometryCollectionComponent* SlabGC = Stage3_TargetSlabGC.Get();
		if (IsValid(SlabGC) && SlabGC->IsRegistered() && !SlabGC->IsBeingDestroyed())
		{
			const FVector P = Stage3_TargetSlabPunchPoint + FVector(
				Stage3Stream.FRandRange(-25.f, 25.f),
				Stage3Stream.FRandRange(-25.f, 25.f),
				Stage3Stream.FRandRange(-15.f, 15.f)
			);

			ApplyStrainToGC(SlabGC, P, Stage3_PunchRadius, Stage3_PunchMag, 1);
		}
	}

	// 7) PhaseC: 2차 분해는 "아주 약하게 1회"
	if (bPhaseC)
	{
		const int32 Extra = FMath::Min(1, GCSlabs.Num());
		for (int32 i = 0; i < Extra; ++i)
		{
			UGeometryCollectionComponent* G = GCSlabs[i].Get();
			if (!IsValid(G) || !G->IsRegistered() || G->IsBeingDestroyed()) continue;

			const FVector Center = G->Bounds.Origin;
			ApplyStrainToGC(G, Center + FVector(0, 0, -40.f), 160.f, 60.f, 1);
		}
	}
}

void AStructGraphManager::EnablePhysicsForGCArray_NoRecreate(
	const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr,
	bool bEnableSim, bool bEnableGrav)
{
	ForEachValidGC(Arr, [&](UGeometryCollectionComponent* GC)
		{
			if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed()) return;

			if (GC->Mobility != EComponentMobility::Movable)
			{
				GC->SetMobility(EComponentMobility::Movable);
			}

			GC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			GC->SetCollisionProfileName(TEXT("PhysicsActor"));
			GC->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

			GC->SetSimulatePhysics(bEnableSim);
			GC->SetEnableGravity(bEnableGrav);

			if (bEnableSim)
			{
				GC->WakeAllRigidBodies();
			}
		});
}

static float Clamp01(float V) { return FMath::Clamp(V, 0.f, 1.f); }

void AStructGraphManager::ApplySlabRampForce_BottomUp(float T)
{
	// GCSlabs는 BuildGCCache에서 Z 오름차순 정렬되어 있다고 가정 (낮은 슬래브가 앞쪽)
	const int32 N = GCSlabs.Num();
	if (N <= 0) return;

	// 아직 시작 전이면 아무 것도 하지 않음
	if (T < Stage3_SlabRampStartTime) return;

	// 0~1: 램프업 진행률 (시간에 따라)
	const float TimeAlpha = Clamp01((T - Stage3_SlabRampStartTime) / FMath::Max(0.01f, (Stage3_SlabRampEndTime - Stage3_SlabRampStartTime)));

	// "몇 개 슬래브까지" 힘을 줄지: 아래에서 위로 확장
	// 최소 1개는 항상 포함
	const int32 ActiveCount = FMath::Clamp(1 + FMath::FloorToInt(TimeAlpha * float(N - 1)), 1, N);

	// 아래쪽은 더 강하게, 위로 갈수록 약하게(초반에 위가 먼저 무너지지 않게)
	for (int32 i = 0; i < ActiveCount; ++i)
	{
		UGeometryCollectionComponent* SlabGC = GCSlabs[i].Get();
		if (!IsValid(SlabGC) || !SlabGC->IsRegistered() || SlabGC->IsBeingDestroyed()) continue;

		// 0(가장 아래) -> 1(가장 위)로 갈수록 약하게
		const float HeightAlpha = (N <= 1) ? 0.f : (float(i) / float(N - 1));

		// 시간 램프업 + 높이 감쇠
		const float ForceAlpha = TimeAlpha * (1.f - 0.65f * HeightAlpha);

		// 아래 방향 힘(가짜 중력)
		const FVector F = FVector(0, 0, -1.f) * (Stage3_SlabForceMax * ForceAlpha);

		SlabGC->AddForce(F, NAME_None, true);
		SlabGC->WakeAllRigidBodies();
	}
}

UGeometryCollectionComponent* AStructGraphManager::FindGCByOwnerNetId(int32 NetId) const
{
	if (NetId < 0) return nullptr;

	auto MatchIn = [&](const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr) -> UGeometryCollectionComponent*
		{
			for (const auto& W : Arr)
			{
				UGeometryCollectionComponent* GC = W.Get();
				if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed()) continue;

				AActor* OA = GC->GetOwner();
				if (!IsValid(OA) || OA->IsActorBeingDestroyed()) continue;

				// NetID
				static const FName NetIdName(TEXT("NetId"));
				if (FProperty* P = OA->GetClass()->FindPropertyByName(NetIdName))
				{
					if (FIntProperty* IntP = CastField<FIntProperty>(P))
					{
						const int32 Value = IntP->GetPropertyValue_InContainer(OA);
						if (Value == NetId)
						{
							return GC;
						}
					}
				}
			}
			return nullptr;
		};

	// 기둥/슬래브/벽
	if (UGeometryCollectionComponent* R = MatchIn(GCColumns)) return R;
	if (UGeometryCollectionComponent* R = MatchIn(GCSlabs)) return R;
	if (UGeometryCollectionComponent* R = MatchIn(GCWalls)) return R;
	return nullptr;
}

void AStructGraphManager::Net_StartStage3(const FStage3NetStart& Info)
{
	
	BuildGCCache();

	// 기존 타이머/루프 정리 
	GetWorldTimerManager().ClearTimer(Stage2Timer);
	GetWorldTimerManager().ClearTimer(Stage3SlabDelayHandle);
	GetWorldTimerManager().ClearTimer(Stage3ContinuousHandle);

	StopEarthquake(); 

	QuakeStage = EQuakeStage::Stage3;

	// 파라미터 적용
	Stage3_TotalDuration = Info.TotalDuration;
	Stage3_ShakeImpulse = Info.ShakeImpulse;

	Stage3Stream.Initialize(Info.Seed);

	Stage3_WeakColumnGC = FindGCByOwnerNetId(Info.WeakColumnNetID);
	Stage3_TargetSlabGC = FindGCByOwnerNetId(Info.TargetSlabNetID);

	Stage3_TargetSlabPunchPoint = FVector::ZeroVector;
	if (UGeometryCollectionComponent* Col = Stage3_WeakColumnGC.Get())
	{
		if (AActor* ColA = Col->GetOwner())
		{
			FVector Origin, Extent;
			ColA->GetActorBounds(true, Origin, Extent);
			Stage3_TargetSlabPunchPoint = Origin + FVector(0, 0, Extent.Z * 0.45f);
		}
	}

	// 쉐이크
	PlayShake(QuakeStage3LongShakeClass, Stage3LongScale);

	// Stage3 시작
	StartStage3Continuous();
}
void AStructGraphManager::EnsureChaosBodiesBuiltOnce(UGeometryCollectionComponent* GC)
{
	if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed()) return;

	const TWeakObjectPtr<UGeometryCollectionComponent> Key(GC);
	if (Stage3_RecreatedOnce.Contains(Key)) return;
	Stage3_RecreatedOnce.Add(Key);

	GC->SetSimulatePhysics(false);
	GC->SetEnableGravity(false);
	GC->RecreatePhysicsState();

	GC->SetSimulatePhysics(true);
	GC->SetEnableGravity(false);

	if (FBodyInstance* BI = GC->GetBodyInstance())
	{
		BI->SetEnableGravity(false);
		BI->WakeInstance();
	}

	GC->WakeAllRigidBodies();
}
void AStructGraphManager::ApplyManualGravityToGCArray(
	const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr,
	float Scale)
{
	UWorld* W = GetWorld();
	if (!IsValid(W)) return;

	const FVector Accel(0.f, 0.f, W->GetGravityZ() * Scale); // 추가 하강 가속

	ForEachValidGC(Arr, [&](UGeometryCollectionComponent* GC)
		{
			if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed()) return;

			// Sim ON
			if (!GC->IsSimulatingPhysics())
			{
				GC->SetSimulatePhysics(true);
				/*if (!GC->IsSimulatingPhysics())
				{
					GC->RecreatePhysicsState();
					GC->SetSimulatePhysics(true);
				}*/
			}

			// gravity true
			GC->SetEnableGravity(true);

			if (FBodyInstance* BI = GC->GetBodyInstance())
			{
				BI->SetEnableGravity(true);
				BI->WakeInstance();
			}

			GC->AddForce(Accel, NAME_None, true);

			GC->WakeAllRigidBodies();
		});
}

void AStructGraphManager::StartStage3ManualGravity(float Duration)
{
	if (!bStage3_UseManualGravity) return;

	UWorld* W = GetWorld();
	if (!IsValid(W)) return;

	Stage3ManualGravityEndTime = W->GetTimeSeconds() + FMath::Max(0.1f, Duration);

	GetWorldTimerManager().ClearTimer(Stage3ManualGravityHandle);
	GetWorldTimerManager().SetTimer(
		Stage3ManualGravityHandle,
		this,
		&AStructGraphManager::Stage3_ManualGravityTick,
		Stage3_TickInterval,   
		true
	);
}

void AStructGraphManager::Stage3_ManualGravityTick()
{
	UWorld* W = GetWorld();
	if (!IsValid(W))
	{
		GetWorldTimerManager().ClearTimer(Stage3ManualGravityHandle);
		return;
	}

	const float Now = W->GetTimeSeconds();
	if (Now >= Stage3ManualGravityEndTime)
	{
		GetWorldTimerManager().ClearTimer(Stage3ManualGravityHandle);

		ForEachValidGC(GCWalls, [](UGeometryCollectionComponent* GC) { GC->SetEnableGravity(true); GC->WakeAllRigidBodies(); });
		ForEachValidGC(GCColumns, [](UGeometryCollectionComponent* GC) { GC->SetEnableGravity(true); GC->WakeAllRigidBodies(); });
		ForEachValidGC(GCSlabs, [](UGeometryCollectionComponent* GC) { GC->SetEnableGravity(true); GC->WakeAllRigidBodies(); });

		return;
	}

	ApplyManualGravityToGCArray(GCWalls, Stage3_ManualGravityScale);
	ApplyManualGravityToGCArray(GCColumns, Stage3_ManualGravityScale);
	ApplyManualGravityToGCArray(GCSlabs, Stage3_ManualGravityScale);
}

void AStructGraphManager::AddGravityAssist(UGeometryCollectionComponent* GC, float Duration)
{
	if (!bStage3_GravityAssist) return;
	UWorld* W = GetWorld();
	if (!IsValid(W) || !IsValid(GC)) return;

	GravityAssistUntil.FindOrAdd(GC) = W->GetTimeSeconds() + FMath::Max(0.1f, Duration);
}

void AStructGraphManager::TickGravityAssist()
{
	if (!bStage3_GravityAssist) return;

	UWorld* W = GetWorld();
	if (!IsValid(W) || !Stage3_GravityField) return;

	const float Now = W->GetTimeSeconds();

	const float Mag = FMath::Abs(W->GetGravityZ()) * Stage3_GravityAssistScale;
	Stage3_GravityField->Direction = FVector(0.f, 0.f, -1.f); 
	Stage3_GravityField->Magnitude = Mag;

	for (auto It = GravityAssistUntil.CreateIterator(); It; ++It)
	{
		UGeometryCollectionComponent* GC = It.Key().Get();
		if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed() || Now > It.Value())
		{
			It.RemoveCurrent();
			continue;
		}

		// Sim
		if (!GC->IsSimulatingPhysics())
		{
			GC->SetSimulatePhysics(true);
		}

		GC->SetEnableGravity(true);
		if (FBodyInstance* BI = GC->GetBodyInstance())
		{
			BI->SetEnableGravity(true);
			BI->WakeInstance();
		}

		GC->ApplyPhysicsField(
			true,
			EGeometryCollectionPhysicsTypeEnum::Chaos_LinearForce,
			nullptr,
			Stage3_GravityField
		);

		GC->WakeAllRigidBodies();
	}
}

void AStructGraphManager::DumpGCCache(const FString& Why)
{
	auto DumpArr = [&](const TCHAR* Label, const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr)
		{
			UE_LOG(LogTemp, Warning, TEXT("---- Dump %s (%s) Count=%d ----"), Label, *Why, Arr.Num());

			for (int32 i = 0; i < Arr.Num(); ++i)
			{
				UGeometryCollectionComponent* GC = Arr[i].Get();
				if (!IsValid(GC))
				{
					UE_LOG(LogTemp, Warning, TEXT("[%s][%d] GC=null"), Label, i);
					continue;
				}

				AActor* OA = GC->GetOwner();
				UE_LOG(LogTemp, Warning,
					TEXT("[%s][%d] Owner=%s Sim=%d Grav=%d Awake=%d Reg=%d Loc=%s"),
					Label, i,
					*GetNameSafe(OA),
					GC->IsSimulatingPhysics() ? 1 : 0,
					GC->IsGravityEnabled() ? 1 : 0,
					GC->IsAnyRigidBodyAwake() ? 1 : 0,
					GC->IsRegistered() ? 1 : 0,
					*GC->GetComponentLocation().ToString()
				);
			}
		};

	DumpArr(TEXT("WALL"), GCWalls);
	DumpArr(TEXT("COL"), GCColumns);
	DumpArr(TEXT("SLAB"), GCSlabs);
}
void AStructGraphManager::DisableAllProxies()
{
	auto DisableProxyOn = [&](const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr)
		{
			for (const auto& W : Arr)
			{
				UGeometryCollectionComponent* GC = W.Get();
				if (!IsValid(GC)) continue;

				AActor* Owner = GC->GetOwner();
				if (!IsValid(Owner)) continue;

				TArray<UActorComponent*> Comps;
				Owner->GetComponents(UStaticMeshComponent::StaticClass(), Comps);

				for (UActorComponent* C : Comps)
				{
					UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C);
					if (!IsValid(SM)) continue;

					if (SM->ComponentHasTag(TEXT("GC_PROXY")))
					{
						SM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					}
				}
			}
		};

	DisableProxyOn(GCWalls);
	DisableProxyOn(GCColumns);
	DisableProxyOn(GCSlabs);
}