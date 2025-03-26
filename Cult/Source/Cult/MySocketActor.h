#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Containers/Queue.h"
#include "ReplicatedPhysicsBlock.h"
#include "PoliceCharacter.h"
#include <winsock2.h>
#include "MySocketActor.generated.h"

UENUM(BlueprintType)
enum class EAnimationState : uint8
{
	Idle UMETA(DisplayName = "IDLE"),
	Walk UMETA(DisplayName = "Walk"),
	Crouch UMETA(DisplayName = "Crouch")
};

USTRUCT(BlueprintType)
struct FPoliceCharacterState
{
	GENERATED_BODY()
	int32 PlayerID;
	// 위치
	float PositionX;
	float PositionY;
	float PositionZ;
	// 회전
	float RotationPitch;
	float RotationYaw;
	float RotationRoll;
	// 속도
	float VelocityX;
	float VelocityY;
	float VelocityZ;
	float Speed;

	bool bIsCrouching;
	bool bIsAiming;
	bool bIsAttacking;
	EWeaponType CurrentWeapon;
	bool bIsPakour;
	bool bIsNearEnoughToPakour;
	EVaultingType CurrentVaultType;
	bool bIsOneHand;
	bool bIsTwoHand;
	bool bIsFlip;
	bool bIsShooting;
};

USTRUCT(BlueprintType)
struct FCultistCharacterState
{
	GENERATED_BODY()
	int32 PlayerID;
	// 위치
	float PositionX;
	float PositionY;
	float PositionZ;
	// 회전
	float RotationPitch;
	float RotationYaw;
	float RotationRoll;
	// 속도
	float VelocityX;
	float VelocityY;
	float VelocityZ;
	float Speed;

	bool bIsCrouching;
};

UCLASS()
class CULT_API AMySocketActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMySocketActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	SOCKET ServerSocket;
	FPoliceCharacterState ServerState;
	APoliceCharacter* ServerCharacter;
	TQueue<FCultistCharacterState> ReceivedDataQueue;
	TArray<SOCKET> ClientSockets;
	TMap<SOCKET, ACharacter*> ClientCharacters;
	TMap<SOCKET, FCultistCharacterState> ClientStates;
	TMap<int32, AReplicatedPhysicsBlock*> BlockMap;
	TMap<int32, FTransform> BlockTransforms;
	TArray<FVector> ImpactLocations;
	UParticleSystem* ImpactParticle;
	FCriticalSection ClientSocketsMutex;
	bool bIsRunning = false;
	uint8 cultistHeader = 0x00;
	uint8 objectHeader = 0x01;
	uint8 policeHeader = 0x10;
	uint8 particleHeader = 0x11;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool InitializeServer(int32 Port);
	void LogAndCleanupSocketError(const TCHAR* ErrorMessage);
	void AcceptClientAsync();
	void InitializeBlocks();
	void SendCultistData(SOCKET TargetSocket);
	void SendPoliceData(SOCKET TargetSocket);
	void SendObjectData(int32 BlockID, FTransform NewTransform);
	void SendParticleData(SOCKET TargetSocket, FVector ImpactLoc);
	FPoliceCharacterState GetServerCharacterState();
	void ReceiveData(SOCKET ClientSocket);
	void ProcessPlayerData(SOCKET ClientSocket, char* Buffer, int32 BytesReceived);
	void SpawnClientCharacter(SOCKET ClientSocket, const FCultistCharacterState& State);
	void SpawnOrUpdateClientCharacter(SOCKET ClientSocket, const FCultistCharacterState& State);
	void UpdateCharacterState(ACharacter* Character, const FCultistCharacterState& State);
	void UpdateAnimInstanceProperties(UAnimInstance* AnimInstance, const FCultistCharacterState& State);
	void CheckImpactEffect();
	void SpawnImpactEffect(const FVector& ImpactLocation);
	void CloseClientSocket(SOCKET ClientSocket);
	void CloseAllClientSockets();
	void CloseServerSocket();

	UPROPERTY()
	bool bIsServer = true;
};
