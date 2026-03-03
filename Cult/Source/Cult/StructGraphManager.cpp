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

AStructGraphManager::AStructGraphManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AStructGraphManager::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("[StructGraph] BeginPlay: %s"), *GetName());

	BuildGCCache();

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


	//EnablePhysicsForTaggedGC(GCWallTag, true, true);
	EnablePhysicsForGCArray(GCWalls, true, true);
	/*EnablePhysicsForGCArray(GCColumns, true, true);
	EnablePhysicsForGCArray(GCSlabs, true, true);*/

	ApplyDampingToTaggedGC(GCWallTag, Stage2LinearDamping, Stage2AngularDamping);


	SeismicBase = Stage1_SeismicBase;
	SeismicOmega = Stage1_Omega;
	StartEarthquake();

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
	Stage3_TargetSlabGC.Reset();
	Stage3_TargetSlabPunchPoint = FVector::ZeroVector;

	BuildGCCache();

	GetWorldTimerManager().ClearTimer(Stage2Timer);
	GetWorldTimerManager().ClearTimer(Stage3WaveTimer);

	Stage2Stream.Initialize(Stage2Seed);

	bDrawDebug = false;
	bStage3Released = false;
	Stage3WaveElapsed = 0.f;

	//EnablePhysicsForTaggedGC(GCWallTag, true, true);
	//EnablePhysicsForTaggedGC(FName("GC_COLUMN"), true, true);
	//EnablePhysicsForTaggedGC(FName("GC_SLAB"), true, true);

	EnablePhysicsForGCArray(GCWalls, true, true);
	//EnablePhysicsForGCArray(GCSlabs, true, true);

	ApplyDampingToTaggedGC(GCWallTag, Stage3LinearDamping, Stage3AngularDamping);

	UE_LOG(LogTemp, Warning, TEXT("[Quake] Stage3 Start (Ending 5s)"));
	SeismicBase = Stage3_SeismicBase;
	SeismicOmega = Stage3_Omega;

	StopEarthquake();
	StartEarthquake();

	PlayShake(QuakeStage3LongShakeClass, Stage3LongScale);

	Stage3_TickWave();

	GetWorldTimerManager().SetTimer(
		Stage3WaveTimer,
		this,
		&AStructGraphManager::Stage3_TickWave,
		Stage3_WaveInterval,
		true
	);
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

	if (GCWalls.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Stage2Pulse] GCWalls cache is empty. Call BuildGCCache() in BeginPlay."));
		return;
	}

	const int32 PickCount = FMath::Min(Stage2_LocalDamageCount, GCWalls.Num());

	for (int32 i = 0; i < PickCount; ++i)
	{
		UGeometryCollectionComponent* GCComp = PickRandomValidGC(GCWalls, Stage2Stream);
		if (!IsValid(GCComp)) continue;

		AActor* A = GCComp->GetOwner();
		if (!IsValid(A)) continue;

		FVector Origin, Extent;
		A->GetActorBounds(true, Origin, Extent);

		const FVector HitPoint = Origin + FVector(
			Stage2Stream.FRandRange(-Extent.X * 0.5f, Extent.X * 0.5f),
			Stage2Stream.FRandRange(-Extent.Y * 0.5f, Extent.Y * 0.5f),
			Stage2Stream.FRandRange(-Extent.Z * 0.3f, Extent.Z * 0.3f)
		);

		ApplyStrainToGC(GCComp, HitPoint, Stage2_Str_Radius, Stage2_Str_Magnitude, 2);

		if (Stage2_ImpulseScale > 0.f && Stage2_KickStrength > 0.f)
		{
			const FVector KickDir = FVector(
				Stage2Stream.FRandRange(-1.f, 1.f),
				Stage2Stream.FRandRange(-1.f, 1.f),
				0.f
			).GetSafeNormal();

			const float KickStrength = Stage2_KickStrength * Stage2_ImpulseScale;
			const FVector KickLocation = Origin - FVector(0, 0, Extent.Z * 0.8f);

			GCComp->AddImpulseAtLocation(KickDir * KickStrength, KickLocation, NAME_None);
		}

		OnNodeYield(GCComp, 1.0f + Stage2_LocalDamageStrength);
	}

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

void AStructGraphManager::Stage3_TickWave()
{
	Stage3WaveElapsed += Stage3_WaveInterval;

	if (Stage3WaveElapsed >= Stage3_EndDuration)
	{
		GetWorldTimerManager().ClearTimer(Stage3WaveTimer);
		UE_LOG(LogTemp, Warning, TEXT("[Stage3] End"));
		return;
	}

	// prune dead pointers (prevents invalid access during late Stage3)
	GCWalls.RemoveAll([](const TWeakObjectPtr<UGeometryCollectionComponent>& P) { return !P.IsValid(); });
	GCColumns.RemoveAll([](const TWeakObjectPtr<UGeometryCollectionComponent>& P) { return !P.IsValid(); });
	GCSlabs.RemoveAll([](const TWeakObjectPtr<UGeometryCollectionComponent>& P) { return !P.IsValid(); });

	// 1) wall strain (light)
	if (GCWalls.Num() > 0)
	{
		const int32 HitCount = FMath::Min(2, GCWalls.Num());

		for (int32 i = 0; i < HitCount; ++i)
		{
			UGeometryCollectionComponent* GCComp = GCWalls[i].Get();
			if (!IsValid(GCComp) || !GCComp->IsRegistered() || GCComp->IsBeingDestroyed()) continue;

			AActor* A = GCComp->GetOwner();
			if (!IsValid(A) || A->IsActorBeingDestroyed()) continue;

			FVector Origin, Extent;
			A->GetActorBounds(true, Origin, Extent);

			const FVector Base = Origin - FVector(0, 0, Extent.Z * 0.7f);

			const int32 Idx = Stage3WallPtIdx++ % 3;

			FVector HitPoint = Base;
			if (Idx == 0) HitPoint += FVector(-Extent.X * 0.35f, 0.f, 0.f);
			if (Idx == 2) HitPoint += FVector(+Extent.X * 0.35f, 0.f, 0.f);

			ApplyStrainToGC(GCComp, HitPoint, Stage3_Wall_Radius, Stage3_Wall_Mag, Stage3_Wall_Iter);
		}
	}

	// 2) release once
	if (!bStage3Released && Stage3WaveElapsed >= Stage3_ReleaseTime)
	{
		bStage3Released = true;

		EnablePhysicsForGCArray(GCColumns, true, true);



		UGeometryCollectionComponent* ColGC = PickLowestColumnGC();
		if (IsValid(ColGC) && ColGC->IsRegistered() && !ColGC->IsBeingDestroyed())
		{
			// check
			UE_LOG(LogTemp, Warning, TEXT("[Stage3] ColGC Sim=%d Grav=%d Reg=%d Awake=%d"),
				ColGC->IsSimulatingPhysics() ? 1 : 0,
				ColGC->IsGravityEnabled() ? 1 : 0,
				ColGC->IsRegistered() ? 1 : 0,
				ColGC->IsAnyRigidBodyAwake() ? 1 : 0);

			AActor* ColActor = ColGC->GetOwner();
			EnablePhysicsForGCArray({ ColGC }, true, true);
			if (IsValid(ColActor) && !ColActor->IsActorBeingDestroyed())
			{
				FVector Origin, Extent;
				ColActor->GetActorBounds(true, Origin, Extent);

				const float ColRadius = 90.f;
				const float ColMag = 120000.f;
				const int32 ColIter = 3;

				for (int32 k = 0; k < 3; ++k)
				{
					FVector HitPoint = Origin - FVector(0, 0, Extent.Z * 0.7f);
					HitPoint += FVector(
						Stage2Stream.FRandRange(-15.f, 15.f),
						Stage2Stream.FRandRange(-15.f, 15.f),
						Stage2Stream.FRandRange(-10.f, 10.f)
					);

					ApplyStrainToGC(ColGC, HitPoint, ColRadius, ColMag, ColIter);
				}

				UE_LOG(LogTemp, Warning, TEXT("[Stage3] Broke Column: %s"), *ColActor->GetName());

				// cache slab target once (prevents late-timer invalid search)
				if (!Stage3_TargetSlabGC.IsValid())
				{
					UGeometryCollectionComponent* FoundSlab = FindNearestSlabGC(Origin);
					if (IsValid(FoundSlab) && FoundSlab->IsRegistered() && !FoundSlab->IsBeingDestroyed())
					{
						Stage3_TargetSlabGC = FoundSlab;

						Stage3_TargetSlabPunchPoint = Origin + FVector(0, 0, Extent.Z * 0.45f);
						Stage3_TargetSlabPunchPoint += FVector(
							Stage2Stream.FRandRange(-20.f, 20.f),
							Stage2Stream.FRandRange(-20.f, 20.f),
							Stage2Stream.FRandRange(-10.f, 10.f)
						);

						if (AActor* SA = FoundSlab->GetOwner())
						{
							UE_LOG(LogTemp, Warning, TEXT("[Stage3] Target Slab Selected: %s"), *SA->GetName());
						}
					}
				}

				UGeometryCollectionComponent* SlabGC = Stage3_TargetSlabGC.Get();
				EnablePhysicsForGCArray({ SlabGC }, true, true);
				if (IsValid(SlabGC) && SlabGC->IsRegistered() && !SlabGC->IsBeingDestroyed())
				{
					// check
					UE_LOG(LogTemp, Warning, TEXT("[Stage3] SlabGC Sim=%d Grav=%d Reg=%d Awake=%d"),
						SlabGC->IsSimulatingPhysics() ? 1 : 0,
						SlabGC->IsGravityEnabled() ? 1 : 0,
						SlabGC->IsRegistered() ? 1 : 0,
						SlabGC->IsAnyRigidBodyAwake() ? 1 : 0);

					const float PunchRadius = 120.f;
					const float PunchMag = 90000.f;
					const int32 PunchIter = 3;

					for (int32 p = 0; p < 3; ++p)
					{
						if (!IsValid(SlabGC) || !SlabGC->IsRegistered() || SlabGC->IsBeingDestroyed())
						{
							break;
						}

						const FVector P = Stage3_TargetSlabPunchPoint + FVector(
							Stage2Stream.FRandRange(-15.f, 15.f),
							Stage2Stream.FRandRange(-15.f, 15.f),
							Stage2Stream.FRandRange(-5.f, 5.f)
						);

						ApplyStrainToGC(SlabGC, P, PunchRadius, PunchMag, PunchIter);
					}

					if (AActor* SA = SlabGC->GetOwner())
					{
						UE_LOG(LogTemp, Warning, TEXT("[Stage3] Punch Slab: %s"), *SA->GetName());
					}
				}
			}
		}

		// delayed slab physics ON (subset to avoid GPU spike)
		TWeakObjectPtr<AStructGraphManager> WeakThis(this);

		GetWorldTimerManager().SetTimer(Stage3SlabDelayHandle, FTimerDelegate::CreateLambda([WeakThis]()
			{
				if (!WeakThis.IsValid()) return;
				if (!IsValid(WeakThis->GetWorld())) return;

				WeakThis->GCSlabs.RemoveAll([](const TWeakObjectPtr<UGeometryCollectionComponent>& P) { return !P.IsValid(); });

				const int32 MaxEnable = FMath::Min(12, WeakThis->GCSlabs.Num());

				TArray<TWeakObjectPtr<UGeometryCollectionComponent>> Subset;
				Subset.Reserve(MaxEnable);
				for (int32 i = 0; i < MaxEnable; ++i)
				{
					Subset.Add(WeakThis->GCSlabs[i]);
				}

				WeakThis->EnablePhysicsForGCArray(Subset, true, true);
				UE_LOG(LogTemp, Warning, TEXT("[Stage3] Slabs physics ON subset=%d/%d"), MaxEnable, WeakThis->GCSlabs.Num());

			}), 0.5f, false);
	}
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

// ===== EnablePhysicsForGCArray (GPU-safe) =====

void AStructGraphManager::EnablePhysicsForGCArray(
	const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr,
	bool bEnableSim, bool bEnableGrav)
{
	ForEachValidGC(Arr, [&](UGeometryCollectionComponent* GC)
		{
			if (!IsValid(GC) || !GC->IsRegistered() || GC->IsBeingDestroyed()) return;

			// Mobility 강제 (Static이면 Chaos에서 이상 동작 가능)
			if (GC->Mobility != EComponentMobility::Movable)
			{
				GC->SetMobility(EComponentMobility::Movable);
			}

			GC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			GC->SetCollisionProfileName(TEXT("PhysicsActor"));
			GC->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

			// 현재 상태 저장
			const bool bWasSim = GC->IsSimulatingPhysics();

			// 1) Sim이 꺼져있던 애를 "처음 켤 때만" Recreate (스냅백 방지 핵심)
			if (!bWasSim && bEnableSim)
			{
				GC->SetSimulatePhysics(false);
				GC->RecreatePhysicsState();
			}

			// 2) 목표 상태 적용
			GC->SetSimulatePhysics(bEnableSim);
			GC->SetEnableGravity(bEnableGrav);

			// 3) BodyInstance 레벨로 중력/웨이크 강제
			if (FBodyInstance* BI = GC->GetBodyInstance())
			{
				if (bEnableGrav) BI->SetEnableGravity(true);
				if (bEnableSim)
				{
					BI->WakeInstance();
				}
			}

			// 4) Sleep 방지: 깨우기 + 약한 임펄스
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
	for (const auto& W : GCColumns)
	{
		UGeometryCollectionComponent* GC = W.Get();
		if (IsValid(GC) && GC->IsRegistered() && !GC->IsBeingDestroyed())
		{
			AActor* OA = GC->GetOwner();
			if (IsValid(OA) && !OA->IsActorBeingDestroyed())
			{
				return GC;
			}
		}
	}
	return nullptr;
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