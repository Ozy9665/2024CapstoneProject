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

	// 우선 그래프 확인
	//SolveLoadsAndDamage();

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

void AStructGraphManager::BuildSupportGraph()
{
	// Id->Index 맵
	TMap<int32, int32> IdToIndex;
	IdToIndex.Reserve(Nodes.Num());
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		IdToIndex.Add(Nodes[i].Id, i);
	}

	// Supports 초기화
	for (FStructGraphNode& N : Nodes)
	{
		if (N.Type == EStructNodeType::Slab)
		{
			N.Supports.Reset();
			N.SupportWeights.Reset();
		}
	}

	// 후보 지지대(기둥/벽)
	TArray<int32> SupporterIds;
	SupporterIds.Reserve(Nodes.Num());
	for (const FStructGraphNode& N : Nodes)
	{
		if (N.Type == EStructNodeType::Column || N.Type == EStructNodeType::Wall)
		{
			SupporterIds.Add(N.Id);
		}
	}

	// 슬래브별 지지대 탐색
	for (FStructGraphNode& Slab : Nodes)
	{
		if (Slab.Type != EStructNodeType::Slab || !Slab.Comp)
			continue;

		const FBox SlabBox = GetCompBox(Slab.Comp);
		const FVector SlabCenter = SlabBox.GetCenter();
		const float SlabBottomZ = SlabBox.Min.Z;
		const int32 SlabFloor = Slab.FloorIndex;

		// 후보 지지대 탐색
		for (int32 Sid : SupporterIds)
		{
			const int32* SupIdx = IdToIndex.Find(Sid);
			if (!SupIdx) continue;

			FStructGraphNode& Sup = Nodes[*SupIdx];
			if (!Sup.Comp) continue;

			
			if (Sup.FloorIndex != -1)
			{
				// 같은층 또는 바로 아래층 정도만 허용
				if (!(Sup.FloorIndex == SlabFloor || Sup.FloorIndex == SlabFloor - 1))
					continue;
			}

			const FBox SupBox = GetCompBox(Sup.Comp);

			// 슬래브 바닥 Z가 지지대의 Z범위 안에 들어오면 지지 후보로 인정
			const float Z = SlabBottomZ;
			const bool bZHit =
				(Z >= SupBox.Min.Z - SupportZTolerance) &&
				(Z <= SupBox.Max.Z + SupportZTolerance);

			if (!bZHit)
				continue;

			// XY 겹침
			if (!Overlap2D(SlabBox, SupBox, SupportXYExpand))
				continue;

			// 가중치 계산
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

		// 정규화
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
	//모든 노드 carried load 초기화
	for (FStructGraphNode& N : Nodes)
	{
		N.CarriedLoad = 0.f;
	}
	// Floor별 슬래브를 모아 위에서 아래로 처리할 수 있게 정렬 준비
	TArray<int32> SlabIndices;
	SlabIndices.Reserve(Nodes.Num());

	int32 MaxFloor = -999;
	int32 MinFloor = 999;

	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		if (Nodes[i].Type == EStructNodeType::Slab)
		{
			SlabIndices.Add(i);
			MaxFloor = FMath::Max(MaxFloor, Nodes[i].FloorIndex);
			MinFloor = FMath::Min(MinFloor, Nodes[i].FloorIndex);
		}
	}

	SlabIndices.Sort([&](int32 A, int32 B)
		{
			return Nodes[A].FloorIndex > Nodes[B].FloorIndex; // 높은 층부터
		});

	// 지진파동을 수직 등가하중으로
	auto GetSeismicMultiplier = [&](int32 FloorIndex) -> float
		{
			const float FloorAmp = 1.f + (SeismicFloorAmplify * FMath::Max(0, FloorIndex));
			// 수평 -> 수직 등가 반영 스케일
			return 1.f + (FMath::Abs(SeismicFactor) * SeismicToVerticalScale * FloorAmp);
		};

	// 슬래브 하중을 지지대로 분배 (위->아래)
	for (int32 SlabIdx : SlabIndices)
	{
		FStructGraphNode& Slab = Nodes[SlabIdx];
		if (!Slab.Comp) continue;

		// 슬래브가 실제로 내려보낼 총 하중
		const float SeisMul = GetSeismicMultiplier(Slab.FloorIndex);
		const float TotalDownLoad = (Slab.SelfWeight + Slab.CarriedLoad) * SeisMul;

		// 지지대가 없으면 -> Ground로
		if (Slab.Supports.Num() == 0)
		{
			// MVP에서는 그냥 소멸 처리 (ground)
			continue;
		}

		// weights 배열 길이 확인
		if (Slab.SupportWeights.Num() != Slab.Supports.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("[Load] Slab %d supports/weights mismatch"), Slab.Id);
			continue;
		}

		// 분배
		for (int32 k = 0; k < Slab.Supports.Num(); ++k)
		{
			const int32 SupId = Slab.Supports[k];
			const float W = Slab.SupportWeights[k];
			// Sup Id = Nodes 인덱스
			if (!Nodes.IsValidIndex(SupId)) continue;
			FStructGraphNode& Sup = Nodes[SupId];

			// 지지대가 이미 Failed MVP에서는  받지 못함 처리
			if (Sup.State == EStructDamageState::Failed) continue;

			Sup.CarriedLoad += TotalDownLoad * W;
		}
	}

	// TODO - 하중 완료.
	// 다음 단계 -> Failed 지지대가 생기면 그 하중을 다른 지지대로 재분배 루프
}
