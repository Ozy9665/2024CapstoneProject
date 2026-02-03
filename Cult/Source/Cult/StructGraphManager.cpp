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
	// 슬래브 노드 돌면서,  Supports 연결
	for (FStructGraphNode& Slab : Nodes)
	{
		if (Slab.Type != EStructNodeType::Slab || !Slab.Comp) continue;

		Slab.Supports.Reset();

		const FBox SlabBox = GetCompBox(Slab.Comp);
		const float SlabBottomZ = SlabBox.Min.Z;

		for (const FStructGraphNode& Candidate : Nodes)
		{
			if ((Candidate.Type != EStructNodeType::Column && Candidate.Type != EStructNodeType::Wall) || !Candidate.Comp)
				continue;

			const FBox SupBox = GetCompBox(Candidate.Comp);
			const float SupTopZ = SupBox.Max.Z;

			// Z 접촉 조건 + XY 겹침
			const bool bZ = FMath::Abs(SlabBottomZ - SupTopZ) <= SupportZTolerance;
			const bool bXY = Overlap2D(SlabBox, SupBox, SupportXYExpand);

			if (bZ && bXY)
			{
				Slab.Supports.Add(Candidate.Id);
			}
		}
	}
}

void AStructGraphManager::ComputeLoadsFromScratch(float SeismicFactor)
{
	// CarryLoad 초기화
	for (FStructGraphNode& N : Nodes)
	{
		N.CarriedLoad = 0.f;
	}

	TArray<int32> SlabIds;
	for (const FStructGraphNode& N : Nodes)
	{
		if (N.Type == EStructNodeType::Slab) SlabIds.Add(N.Id);
	}
	SlabIds.Sort([&](int32 A, int32 B)
		{
			return Nodes[A].FloorIndex > Nodes[B].FloorIndex;
		});

	for (int32 SlabId : SlabIds)
	{
		FStructGraphNode& Slab = Nodes[SlabId];
		if (Slab.State == EStructDamageState::Failed || !Slab.Comp) continue;

		// 슬래브의 총 하중
		const float TotalSlabLoad = Slab.SelfWeight + Slab.CarriedLoad;

		// 유효 지지대 + 가중치
		TArray<int32> ValidSupports;
		TArray<float> Weights;
		float SumW = 0.f;

		const FBox SlabBox = GetCompBox(Slab.Comp);

		for (int32 SupId : Slab.Supports)
		{
			FStructGraphNode& Sup = Nodes[SupId];
			if (Sup.State == EStructDamageState::Failed || !Sup.Comp) continue;

			const FBox SupBox = GetCompBox(Sup.Comp);
			const float Area = OverlapArea2D(SlabBox, SupBox, SupportXYExpand);

			if (Area > KINDA_SMALL_NUMBER)
			{
				ValidSupports.Add(SupId);
				Weights.Add(Area);
				SumW += Area;
			}
		}

		// 지지대 없으면 fail
		if (ValidSupports.Num() == 0 || SumW <= KINDA_SMALL_NUMBER)
		{
			Slab.State = EStructDamageState::Failed;
			if (Slab.Comp)
			{
				//Slab.Comp->SetSimulatePhysics(true);
				//Slab.Comp->WakeAllRigidBodies();
				OnNodeFailed(Slab.Comp, Slab.Utilization());
			}
			continue;
		}

		// 지진 가중
		const float EffectiveLoad = TotalSlabLoad * (1.f + SeismicFactor * SeismicToVerticalScale);

		// 지지대로 분배
		for (int32 i = 0; i < ValidSupports.Num(); ++i)
		{
			const float Share = EffectiveLoad * (Weights[i] / SumW);
			Nodes[ValidSupports[i]].CarriedLoad += Share;
		}
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
	// 실패가 발생하면 지지 관계가 달라지고 다음 루프에서 재분배가 자동 반영
	for (int32 Iter = 0; Iter < MaxSolveIterations; ++Iter)
	{
		// 지진 인자(정적 계산일 땐 0)
		const float Seismic = 0.f;

		ComputeLoadsFromScratch(Seismic);

		bool bNewFailures = false;
		UpdateDamageStates(bNewFailures);

		if (!bNewFailures) break; // 수렴
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