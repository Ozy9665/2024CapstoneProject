#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class AMySocketActor;
#include "ReplicatedPhysicsBlock.generated.h"

UCLASS()
class SPAWNACTOR_API AReplicatedPhysicsBlock : public AActor
{
    GENERATED_BODY()

public:
    AReplicatedPhysicsBlock();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* MeshComp;

    int32 BlockID;
    void SetBlockID(int32 NewBlockID);

private:
    // **���������� ���۵� ��ġ (�������� ��ġ ��ȭ�� ����)**
    FVector LastSentLocation = FVector::ZeroVector;
    FRotator LastSentRotation = FRotator::ZeroRotator;
};
