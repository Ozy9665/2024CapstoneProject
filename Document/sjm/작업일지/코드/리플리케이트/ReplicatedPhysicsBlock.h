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
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* MeshComp;

    int32 BlockID;
    void SetBlockID(int32 NewBlockID);

    UPROPERTY(ReplicatedUsing = OnRep_UpdatePhysics, VisibleAnywhere, Category = "Replication")
    FVector ReplicatedLocation;

    UPROPERTY(ReplicatedUsing = OnRep_UpdatePhysics, VisibleAnywhere, Category = "Replication")
    FRotator ReplicatedRotation;

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_UpdatePhysics(FVector NewLocation, FRotator NewRotation);
    void Server_UpdatePhysics_Implementation(FVector NewLocation, FRotator NewRotation);
    bool Server_UpdatePhysics_Validate(FVector NewLocation, FRotator NewRotation);
    bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const;
    UFUNCTION()
    void OnRep_UpdatePhysics();

private:
    // **���������� ���۵� ��ġ (�������� ��ġ ��ȭ�� ����)**
    FVector LastSentLocation = FVector::ZeroVector;
};
