// Fill out your copyright notice in the Description page of Project Settings.


#include "BuildingConnector.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"

// Sets default values
ABuildingConnector::ABuildingConnector()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void ABuildingConnector::BeginPlay()
{
	Super::BeginPlay();
	ScanAndConnectBlocks();
    // 연결 후 블록 상태 확인
    for (AActor* Actor : FoundBlocks)
    {
        UBuildingBlockComponent* Comp = Cast<UBuildingBlockComponent>(Actor->GetComponentByClass(UBuildingBlockComponent::StaticClass()));
        if (Comp)
        {
            UE_LOG(LogTemp, Warning, TEXT("[%s] Connected Block Count: %d"), *Actor->GetName(), Comp->ConnectedBlocks.Num());
        }
    }
}

// Called every frame
void ABuildingConnector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABuildingConnector::ScanAndConnectBlocks()
{
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStaticMeshActor::StaticClass(), FoundBlocks);

    FName TargetTag = FName(*FString::Printf(TEXT("%s"), *TargetBuildingID.ToString()));

    TArray<AActor*> BuildingBlocks;
    for (AActor* Actor : FoundBlocks)
    {
        if (Actor->Tags.Contains(TargetTag))
        {
            BuildingBlocks.Add(Actor);
        }
    }

    // 거리 기반 연결
    for (int32 i = 0; i < BuildingBlocks.Num(); ++i)
    {
        AActor* A = BuildingBlocks[i];
        for (int32 j = i + 1; j < BuildingBlocks.Num(); ++j)
        {
            AActor* B = BuildingBlocks[j];
            float Dist = FVector::Dist(A->GetActorLocation(), B->GetActorLocation());
            if (Dist <= ConnectionRadius)
            {
                UE_LOG(LogTemp, Log, TEXT("Connected: %s <-> %s (%.1f cm)"), *A->GetName(), *B->GetName(), Dist);

                // 연결 처리 (Component 붙이기 or 리스트에 추가 등)
                UBuildingBlockComponent* CompA = Cast<UBuildingBlockComponent>(A->GetComponentByClass(UBuildingBlockComponent::StaticClass()));
                UBuildingBlockComponent* CompB = Cast<UBuildingBlockComponent>(B->GetComponentByClass(UBuildingBlockComponent::StaticClass()));

                if (CompA && CompB)
                {
                    if (!CompA->ConnectedBlocks.Contains(CompB))
                    {
                        CompA->ConnectedBlocks.Add(CompB);
                    }
                    if (!CompB->ConnectedBlocks.Contains(CompA))
                    {
                        CompB->ConnectedBlocks.Add(CompA);
                    }
                }
                if (!CompA || !CompB)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Failed to get BuildingBlockComponent from %s or %s"), *A->GetName(), *B->GetName());
                }
            }

        }

        
    }

}