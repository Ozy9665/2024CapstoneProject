#include "StructGraphManager.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

AStructGraphManager::AStructGraphManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AStructGraphManager::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("[StructGraph] BeginPlay: %s"), *GetName());
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

	// 초기 상태에서 1회 계산
	SolveLoadsAndDamage();

	// 임시 콜리전 조절 일괄적용
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

	// 상태 리셋
	for (FStructGraphNode& N : Nodes)
	{
		N.State = EStructDamageState::Intact;
		N.CarriedLoad = 0.f;

		if (N.Comp)
		{
			// 물리 끄고 원위치 복귀
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

	// 그래프 다시 계산
	SolveLoadsAndDamage();
}

int32 AStructGraphManager::AddNode(EStructNodeType Type, UPrimitiveComponent* Comp, int32 FloorIndex)
{
	FStructGraphNode Node;
	Node.Id = Nodes.Num();
	Node.Type = Type;
	Node.Comp = Comp;
	Node.FloorIndex = FloorIndex;

	// MVP 기본값
	if (Comp)
	{
		const FBox Box = GetCompBox(Comp);
		const FVector Size = Box.GetSize();
		const float Volume = FMath::Max(1.f, Size.X * Size.Y * Size.Z);

		const float Density = (Type == EStructNodeType::Slab) ? 0.00008f :
			(Type == EStructNodeType::Column) ? 0.00010f : 0.00006f;
		Node.SelfWeight = Volume * Density;

		// 용량: 기둥/벽  강, 슬래브 약
		const float Strength = (Type == EStructNodeType::Column) ? 0.18f :
			(Type == EStructNodeType::Wall) ? 0.12f : 0.08f;
		Node.Capacity = FMath::Max(0.01f, Volume * Strength * 0.00002f);
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

void AStructGraphManager::BuildSupportGraph()
{
	static bool bPrintedOnce = false;   // 디버그 1회만
	const bool bDoDebugPrint = !bPrintedOnce;
	bPrintedOnce = true;


	TMap<int32, int32> IdToIndex;
	IdToIndex.Reserve(Nodes.Num());
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		IdToIndex.Add(Nodes[i].Id, i);
	}
	// 1) Supports 초기화
	for (FStructGraphNode& N : Nodes)
	{
		if (N.Type == EStructNodeType::Slab)
		{
			N.Supports.Reset();
			N.SupportWeights.Reset();
		}
	}

	// 2) 후보 지지대 목록(기둥/벽) Id 모으기
	TArray<int32> SupporterIds;
	SupporterIds.Reserve(Nodes.Num());
	for (const FStructGraphNode& N : Nodes)
	{
		if (N.Type == EStructNodeType::Column || N.Type == EStructNodeType::Wall)
		{
			SupporterIds.Add(N.Id);
		}
	}

	// 3) 각 슬래브에 대해 지지대 찾기
	for (FStructGraphNode& Slab : Nodes)
	{
		if (Slab.Type != EStructNodeType::Slab || !Slab.Comp) continue;

		const FBox SlabBox = GetCompBox(Slab.Comp);
		const FVector SlabCenter = SlabBox.GetCenter();
		const float SlabBottomZ = SlabBox.Min.Z;

		const int32 SlabFloor = Slab.FloorIndex;

		for (int32 Sid : SupporterIds)
		{
			const int32* SupIdx = IdToIndex.Find(Sid);
			if (!SupIdx) continue;
			FStructGraphNode* Sup = &Nodes[*SupIdx];

			if (!Sup->Comp) continue;

			// 미지정일시 우선 통과
			if (Sup->FloorIndex != -1 && Sup->FloorIndex != SlabFloor)
				continue;

			const FBox SupBox = GetCompBox(Sup->Comp);

			const float SupTopZ = SupBox.Max.Z;
			if (FMath::Abs(SupTopZ - SlabBottomZ) > SupportZTolerance) continue;

			if (!Overlap2D(SlabBox, SupBox, SupportXYExpand)) continue;

			// ===== 가중치 계산 =====
			const float Area = OverlapArea2D(SlabBox, SupBox, SupportXYExpand);
			if (Area <= KINDA_SMALL_NUMBER) continue;

			// 거리 가중치
			const FVector SupCenter = SupBox.GetCenter();
			const float Dist2D = FVector::Dist2D(SlabCenter, SupCenter);
			const float Wdist = 1.f / FMath::Pow(Dist2D + WeightDistEpsilon, WeightDistPower);

			// 타입 가중치
			const float Wtype = (Sup->Type == EStructNodeType::Wall) ? WallTypeFactor : ColumnTypeFactor;

			// 각도 가중치
			float Wangle = 1.f;
			if (Sup->Type == EStructNodeType::Column)
			{
				const FVector Up = FVector::UpVector;
				const FVector ColUp = Sup->Comp->GetUpVector().GetSafeNormal();
				const float Dot = FMath::Clamp(FVector::DotProduct(Up, ColUp), 0.f, 1.f);
				Wangle = FMath::Pow(Dot, AnglePower);
			}

			const float Weight = Area * Wdist * Wtype * Wangle;

			if (Weight > KINDA_SMALL_NUMBER)
			{
				Slab.Supports.Add(Sup->Id);
				Slab.SupportWeights.Add(Weight);
			}
		}


		if(bDoDebugPrint)
		{
			if (Slab.Supports.Num() == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[StructGraph] Slab %d has NO supports! Floor=%d Comp=%s"),
					Slab.Id, Slab.FloorIndex, *GetNameSafe(Slab.Comp));
			}

			float SumW = 0.f;
			for (float W : Slab.SupportWeights) SumW += W;

			if (SumW > KINDA_SMALL_NUMBER)
			{
				for (float& W : Slab.SupportWeights)
				{
					W /= SumW; // 합이 1이 되게 정규화
				}
			}

			UE_LOG(LogTemp, Log, TEXT("[StructGraph] Slab %d(Floor=%d) supports=%d sumW=%.3f boxMinZ=%.1f"),
				Slab.Id, Slab.FloorIndex, Slab.Supports.Num(), SumW, SlabBottomZ);
			struct FPair { int32 Id; float W; };
			TArray<FPair> Pairs;
			Pairs.Reserve(Slab.Supports.Num());
			for (int32 i = 0; i < Slab.Supports.Num(); ++i)
			{
				Pairs.Add({ Slab.Supports[i], Slab.SupportWeights[i] });
			}
			Pairs.Sort([](const FPair& A, const FPair& B) { return A.W > B.W; });

			const int32 TopN = FMath::Min(3, Pairs.Num());
			for (int32 i = 0; i < TopN; ++i)
			{
				UE_LOG(LogTemp, Log, TEXT("   -> Sup %d w=%.3f"), Pairs[i].Id, Pairs[i].W);
			}
			float Check = 0.f;
			for (float W : Slab.SupportWeights) Check += W;

			if (!FMath::IsNearlyEqual(Check, 1.f, 0.01f) && Slab.SupportWeights.Num() > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[StructGraph] Slab %d weight normalize check=%.3f (not ~1)"), Slab.Id, Check);
			}
		}
		
		UE_LOG(LogTemp, Log, TEXT("[Support] Slab %d Floor=%d Supports=%d"),
			Slab.Id, Slab.FloorIndex, Slab.Supports.Num());
	}
}

bool AStructGraphManager::UpdateDamageStates(bool& bAnyNewFailures)
{
	bAnyNewFailures = false;

	for (FStructGraphNode& N : Nodes)
	{
		if (!N.Comp) continue;
		if (N.State == EStructDamageState::Failed) continue;

		const float U = N.Utilization();

		if (U >= FailureThreshold)
		{
			N.State = EStructDamageState::Failed;
			bAnyNewFailures = true;

			// 물리 활성
			//N.Comp->SetSimulatePhysics(true);
			//N.Comp->WakeAllRigidBodies();
			OnNodeFailed(N.Comp, U);
		}
		else if (U >= YieldThreshold)
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

	// 시간에 따라 진동
	const float Wave = FMath::Sin(Elapsed * SeismicOmega);

	const float SeismicFactor = SeismicBase * Wave;

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
	UE_LOG(LogTemp, Log, TEXT("[StructGraph] Yield: %s U=%.2f"),
		Comp ? *Comp->GetName() : TEXT("null"), Utilization);
}

void AStructGraphManager::OnNodeFailed_Implementation(UPrimitiveComponent* Comp, float Utilization)
{
	UE_LOG(LogTemp, Warning, TEXT("[StructGraph] Failed: %s U=%.2f"),
		Comp ? *Comp->GetName() : TEXT("null"), Utilization);
}

void AStructGraphManager::StabilizeStructureComponent(UPrimitiveComponent* PC)
{
	if (!PC) return;

	PC->SetSimulatePhysics(false);
	PC->SetEnableGravity(false);

	// 가장 핵심: 물리 충돌을 끊기
	PC->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}


void AStructGraphManager::ComputeLoadsFromScratch(float SeismicFactor)
{
	// TODO: 하중 계산 구현 예정
}