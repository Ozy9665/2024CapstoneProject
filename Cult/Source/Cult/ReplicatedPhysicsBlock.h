#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class AMySocketActor;
#include "ReplicatedPhysicsBlock.generated.h"

UCLASS()
class CULT_API AReplicatedPhysicsBlock : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AReplicatedPhysicsBlock();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* MeshComp;

	int32 BlockID;
	void SetBlockID(int32 NewBlockID);

private:
	// **마지막으로 전송된 위치 (서버에서 위치 변화를 감지)**
	FVector LastSentLocation = FVector::ZeroVector;
	FRotator LastSentRotation = FRotator::ZeroRotator;
};
