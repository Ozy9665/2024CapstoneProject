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
	BuildDownGraph();

	// 1회 계산
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
	// 초기화
	for (FStructGraphNode& N : Nodes)
	{
		N.CarriedLoad = 0.f;
	}

	// BFS 캐시
	TMap<int32, bool> GroundCache;

	// 반복 완화
	const int32 RelaxIters = 8;

	for (int32 It = 0; It < RelaxIters; ++It)
	{
		// 슬래브 -> 지지대로 분배
		for (FStructGraphNode& Slab : Nodes)
		{
			if (Slab.Type != EStructNodeType::Slab) continue;
			if (!Slab.Comp) continue;
			if (Slab.State == EStructDamageState::Failed) continue;

			const float Total = Slab.SelfWeight + Slab.CarriedLoad;

			if (Slab.FloorIndex == 1)
				continue;

			// 유효 지지대만 필터링 (BFS로 Ground 연결성 확인)
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

				// 핵심 Ground까지 연결된 지지대만 인정
				if (!IsConnectedToGround_BFS(SupId, GroundCache)) continue;

				ValidIdx.Add(i);
				SumW += Slab.SupportWeights[i];
			}

			// 지지대가 없으면 다음 단계에서 Utilization로 Fail 유도
			if (ValidIdx.Num() == 0 || SumW <= KINDA_SMALL_NUMBER)
				continue;

			for (int32 i : ValidIdx)
			{
				const int32 SupId = Slab.Supports[i];
				const float W = Slab.SupportWeights[i] / SumW; // 안전 정규화
				Nodes[SupId].CarriedLoad += Total * W;
			}
		}

		// 지지대 -> 아래층 슬래브로 전달 
		for (FStructGraphNode& Sup : Nodes)
		{
			if (!(Sup.Type == EStructNodeType::Column || Sup.Type == EStructNodeType::Wall)) continue;
			if (!Sup.Comp) continue;
			if (Sup.State == EStructDamageState::Failed) continue;

			const float Total = Sup.SelfWeight + Sup.CarriedLoad;

			// 아래 연결이 없으면 기초로 내려간 것으로 간주
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
	}

	// 지진(전단/수평)을 요구량 증가로 MVP 반영
	if (FMath::Abs(SeismicFactor) > KINDA_SMALL_NUMBER)
	{
		const float Seis = FMath::Abs(SeismicFactor);

		for (FStructGraphNode& N : Nodes)
		{
			// 상층 증폭
			const float Amp = 1.f + SeismicFloorAmplify * FMath::Max(0, N.FloorIndex - 1);
			N.CarriedLoad *= (1.f + Seis * SeismicToVerticalScale * Amp);
		}
	}
}

