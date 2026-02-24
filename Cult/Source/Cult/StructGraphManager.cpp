#include "StructGraphManager.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Field/FieldSystemObjects.h"          
#include "Field/FieldSystemComponent.h"

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
	BuildDownGraph();

	//초기 캘리브레이션
	CalibrateCapacitiesFromInitialState();


	// 1회 계산
	SolveLoadsAndDamage();

	CacheBaseCapacities();


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


// BuildSupportGraph
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


// BuildDownGraph
void AStructGraphManager::BuildDownGraph()
{
	// 1) 기둥/벽 Down 초기화
	for (FStructGraphNode& N : Nodes)
	{
		if (N.Type == EStructNodeType::Column || N.Type == EStructNodeType::Wall)
		{
			N.ClearDown();
		}
	}

	// 2) 슬래브 Id 수집
	TArray<int32> SlabIds;
	SlabIds.Reserve(Nodes.Num());
	for (const FStructGraphNode& N : Nodes)
	{
		if (N.Type == EStructNodeType::Slab)
		{
			SlabIds.Add(N.Id);
		}
	}

	// 3) 각 기둥/벽에서 아래층 슬래브 찾기
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

			// Z 조건: 지지대 바닥이 슬래브 상단에 닿는다
			if (FMath::Abs(SupBottomZ - SlabTopZ) > SupportZTolerance)
				continue;

			// XY 겹침
			if (!Overlap2D(SupBox, SlabBox, SupportXYExpand))
				continue;

			const float Area = OverlapArea2D(SupBox, SlabBox, SupportXYExpand);
			if (Area <= KINDA_SMALL_NUMBER)
				continue;

			// 타입 효율
			const float Wtype = (Sup.Type == EStructNodeType::Wall) ? WallTypeFactor : ColumnTypeFactor;

			const float W = Area * Wtype;
			Sup.DownSlabs.Add(Slab.Id);
			Sup.DownWeights.Add(W);
			SumW += W;
		}

		// 정규화
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

		// 실패 노드는 경로에서 제외
		if (Cur.State == EStructDamageState::Failed)
			continue;

		// Ground 성공 판정 :  L1 슬래브 (MVP)
		if (Cur.Type == EStructNodeType::Slab && Cur.FloorIndex == 1)
		{
			Cache.Add(StartNodeId, true);
			return true;
		}

		// 그래프 진행:
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

		// 슬래브는  Yield만 허용 
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

		// Column/Wall은 정상 Fail/Yield 판정
		if (U >= FailStart)
		{
			N.State = EStructDamageState::Failed;
			bAnyNewFailures = true;

			// Fail된 조각만 물리 활성
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

	// 단계 판정
	if (Elapsed < T1) QuakePhase = EQuakePhase::Micro;
	else if (Elapsed < T2) QuakePhase = EQuakePhase::Progress;
	else if (Elapsed < T3) QuakePhase = EQuakePhase::Collapse;
	else
	{
		StopEarthquake3Phase();
		return;
	}

	// 단계별 요구량 설정
	float LocalBase = 0.0f;
	float LocalOmega = SeismicOmega;

	switch (QuakePhase)
	{
	case EQuakePhase::Micro:
		// 약한 흔들림 + 램프업
		LocalBase = FMath::Lerp(0.05f, 0.20f, Elapsed / FMath::Max(0.01f, T1));
		LocalOmega = SeismicOmega * 0.7f;
		break;

	case EQuakePhase::Progress:
		// 손상/파편 유도
		LocalBase = 0.35f;
		LocalOmega = SeismicOmega * 1.0f;
		break;

	case EQuakePhase::Collapse:
		// 큰 진폭 + 약간 빠른 주파수
		LocalBase = 0.55f;
		LocalOmega = SeismicOmega * 1.2f;
		break;

	default:
		break;
	}

	const float Wave = FMath::Sin(Elapsed * LocalOmega);
	const float SeismicFactor = LocalBase * Wave;

	// 해석 + 손상
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

	// 가장 핵심: 물리 충돌을 끊기
	PC->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}


void AStructGraphManager::ComputeLoadsFromScratch(float SeismicFactor)
{
	// 0) 초기화
	for (FStructGraphNode& N : Nodes)
	{
		N.CarriedLoad = 0.f;
	}

	// 1) 슬래브 -> 지지대(기둥/벽)로 "슬래브 자중" 분배
	TMap<int32, bool> GroundCache;

	for (FStructGraphNode& Slab : Nodes)
	{
		if (Slab.Type != EStructNodeType::Slab) continue;
		if (!Slab.Comp) continue;
		if (Slab.State == EStructDamageState::Failed) continue;

		// L1 슬래브는 Ground로 간주: 분배 생략
		if (Slab.FloorIndex == 1) continue;

		const float SlabLoad = Slab.SelfWeight; //MVP: 자중만

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

			// Ground 연결된 지지대만
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

	// 2) 지지대 -> 아래 슬래브로 전달 (DownGraph)
	//    여기서도 "지지대 자중 + 위에서 받은 하중"만 1회 전달
	for (FStructGraphNode& Sup : Nodes)
	{
		if (!(Sup.Type == EStructNodeType::Column || Sup.Type == EStructNodeType::Wall)) continue;
		if (!Sup.Comp) continue;
		if (Sup.State == EStructDamageState::Failed) continue;

		const float Total = Sup.SelfWeight + Sup.CarriedLoad;

		if (Sup.DownSlabs.Num() == 0)
			continue; // 기초로 내려간 것으로 간주

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
			Nodes[DownSlabId].CarriedLoad += Total * W; //아래 슬래브에 "지지대 하중" 누적
		}
	}

	// 지진 요구량 반영
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


// 초기 캘리브레이션
void AStructGraphManager::CalibrateCapacitiesFromInitialState()
{
	// 초기 상태에서 1회 하중 계산 후,
	// Column/Wall의 평균 Utilization을 Target으로 맞추도록 Capacity를 스케일링

	// 1) 한번 계산
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

		// 1) 상층 증폭
		const float Amp = 1.f + SeismicFloorAmplify * FMath::Max(0, Floor - 1);

		// 2) 전단 증폭
		float Soft = 1.f;
		if (Floor == SoftStoryFloor)
		{
			Soft = SoftStoryBoost;
		}

		// 3) 전단 요구량을 등가 수직으로 환산
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

		// Yield 이상부터 누적 손상
		if (U >= YieldStart)
		{
			float Rate = DamageRateYield;
			if (U >= FailStart) Rate = DamageRateFailed;

			// U가 클수록 추가 손상
			const float UFactor = FMath::Clamp((U - YieldStart) / (FailStart - YieldStart + 1e-3f), 0.f, 2.f);

			N.Damage = FMath::Clamp(N.Damage + Rate * (0.5f + UFactor) * DeltaTime, 0.f, 1.f);

			// Damage에 따라 용량 감소 (점진 붕괴)
			const float Loss = CapacityLossAtFullDamage * N.Damage; // 0~CapacityLossAtFullDamage
			N.Capacity = FMath::Max(0.01f, N.BaseCapacity * (1.f - Loss));
		}
		else
		{
			//
		}
	}
}


// 지진

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
	StopEarthquake();
	QuakePhase = EQuakePhase::Idle;
}









// 지진
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
	SeismicBase = Stage1_SeismicBase;     // 기존 변수 사용
	SeismicOmega = Stage1_Omega;

	StartEarthquake(); // 타이머 진동 시작
}

void AStructGraphManager::TriggerStage2()
{
	UE_LOG(LogTemp, Warning, TEXT("[Quake] Stage2 Start (Local strain on GC walls)"));

	// Stage1은 계속 흔들림 유지
	SeismicBase = Stage1_SeismicBase;
	SeismicOmega = Stage1_Omega;
	StartEarthquake();

	// GC_WALL 찾기
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("GC_WALL"), Found);
	UE_LOG(LogTemp, Warning, TEXT("[GC] Found GC_WALL=%d"), Found.Num());

	int32 Applied = 0;

	// 랜덤하게 몇 개만 적용
	for (AActor* A : Found)
	{
		if (Applied >= Stage2_LocalDamageCount) break;
		if (!IsValid(A)) continue;

		// 액터에서 GeometryCollectionComponent 찾기
		UGeometryCollectionComponent* GCComp = A->FindComponentByClass<UGeometryCollectionComponent>();
		if (!IsValid(GCComp)) continue;

		// HitPoint: 일단 바운드 중심(더 자연스럽게 하고 싶으면 랜덤 오프셋)
		FVector Origin, Extent;
		A->GetActorBounds(true, Origin, Extent);

		FVector HitPoint = Origin;
		HitPoint += FVector(
			FMath::FRandRange(-Extent.X * 0.5f, Extent.X * 0.5f),
			FMath::FRandRange(-Extent.Y * 0.5f, Extent.Y * 0.5f),
			FMath::FRandRange(-Extent.Z * 0.3f, Extent.Z * 0.3f)
		);

		ApplyStrainToGC(GCComp, HitPoint, Stage2_Str_Radius, Stage2_Str_Magnitude);

		// 연출 트리거(원하면 같이)
		OnNodeYield(GCComp, 1.0f + Stage2_LocalDamageStrength);

		Applied++;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Quake] Stage2 Applied=%d"), Applied);
}

void AStructGraphManager::TriggerStage3()
{
	UE_LOG(LogTemp, Warning, TEXT("[Quake] Stage3 Start (Chain collapse)"));
	SeismicBase = Stage3_SeismicBase;
	SeismicOmega = Stage3_Omega;

	// Stage3에서는 Fail이 나올 수 있게 Threshold를 살짝 낮추거나,
	// “실패 시 물리 enable”을 켤 수도 있음
	// 지금은 MVP로 이벤트만 확실히 터지게 진행.
	StartEarthquake();
}

// GC 찾기
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

// 데미지 적용
void AStructGraphManager::ApplyDamageToGC(UGeometryCollectionComponent* GC, const FVector& HitPoint, float Damage)
{
	if (!IsValid(GC)) return;

	// 임펄스 방향은 랜덤(지진의 불규칙성)
	const FVector Dir = FMath::VRand().GetSafeNormal();
	const float Impulse = Damage * Stage2_ImpulseScale;

	// Chaos GC: ApplyDamage가 가장 간단한 “국부 파손 트리거”
	GC->ApplyDamage(Damage, HitPoint, Dir, Impulse);
}

void AStructGraphManager::TriggerPulse2()
{
	// Stage2 펄스: GC_WALL 중 1~2개만 찍어서 strain
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("GC_WALL"), Found);
	if (Found.Num() == 0) return;

	const int32 PickCount = FMath::Min(2, Found.Num());
	for (int32 i = 0; i < PickCount; ++i)
	{
		AActor* A = Found[FMath::RandRange(0, Found.Num() - 1)];
		if (!IsValid(A)) continue;

		UGeometryCollectionComponent* GCComp = A->FindComponentByClass<UGeometryCollectionComponent>();
		if (!IsValid(GCComp)) continue;

		FVector Origin, Extent;
		A->GetActorBounds(true, Origin, Extent);

		FVector HitPoint = Origin + FVector(
			FMath::FRandRange(-Extent.X, Extent.X),
			FMath::FRandRange(-Extent.Y, Extent.Y),
			FMath::FRandRange(-Extent.Z * 0.5f, Extent.Z * 0.5f)
		);

		ApplyStrainToGC(GCComp, HitPoint, Stage2_Str_Radius, Stage2_Str_Magnitude);
		OnNodeYield(GCComp, 1.0f + Stage2_LocalDamageStrength);
	}
}
