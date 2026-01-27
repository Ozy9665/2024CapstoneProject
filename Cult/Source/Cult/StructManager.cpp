#include "StructManager.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

AStructManager::AStructManager()
{
	PrimaryActorTick.bCanEverTick = false; 
}

void AStructManager::BeginPlay()
{
	Super::BeginPlay();

	BuildBaseColumns();
	LinkNeighborsByX();
	DrawDebug();
}

void AStructManager::BuildBaseColumns()
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), ColumnTag, Found);

	Nodes.Empty();

	for (AActor* A : Found)
	{
		if (!IsValid(A)) continue;

		FVector Origin, Extent;
		A->GetActorBounds(true, Origin, Extent);

		const float MinZ = Origin.Z - Extent.Z;

		// 바닥에 닿는 기둥 -> Node
		if (MinZ <= BaseColumnMinZThreshold)
		{
			FStructNode N;
			N.Actor = A;
			N.NodeId = Nodes.Num();

			// Capacity 우선 동일하게 시작 
			N.CurrentLoad = 1.0f;
			N.Capacity = 3.0f;

			Nodes.Add(N);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[StructManager] Found Columns=%d, BaseColumns=%d"),
		Found.Num(), Nodes.Num());
}

void AStructManager::LinkNeighborsByX()
{
	// X축으로 정렬
	Nodes.Sort([](const FStructNode& A, const FStructNode& B)
		{
			if (!IsValid(A.Actor) || !IsValid(B.Actor)) return false;
			return A.Actor->GetActorLocation().X < B.Actor->GetActorLocation().X;
		});

	// 정렬 후 NodeId 재부여 -> 정렬순서 ID
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		Nodes[i].NodeId = i;
		Nodes[i].LeftId = (i > 0) ? (i - 1) : -1;
		Nodes[i].RightId = (i < Nodes.Num() - 1) ? (i + 1) : -1;
	}
}

void AStructManager::DrawDebug()
{
	if (!bDrawDebugGraph) return;

	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		AActor* A = Nodes[i].Actor;
		if (!IsValid(A)) continue;

		const FVector P0 = A->GetActorLocation();

		// 노드
		const FString Txt = FString::Printf(TEXT("ID:%d  L:%.1f/%.1f"),
			Nodes[i].NodeId, Nodes[i].CurrentLoad, Nodes[i].Capacity);

		DrawDebugString(GetWorld(), P0 + FVector(0, 0, 120), Txt, nullptr, FColor::Cyan, DebugDuration);

		// 오른쪽 이웃 연결선
		if (Nodes[i].RightId != -1 && Nodes.IsValidIndex(Nodes[i].RightId))
		{
			AActor* B = Nodes[Nodes[i].RightId].Actor;
			if (IsValid(B))
			{
				const FVector P1 = B->GetActorLocation();
				DrawDebugLine(GetWorld(), P0, P1, FColor::Green, false, DebugDuration, 0, DebugLineThickness);
			}
		} 
	}
}
