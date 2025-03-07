#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Containers/Queue.h"
#include "ReplicatedPhysicsBlock.h"
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
struct FCharacterState
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
	// 애니메이션 상태 필드 추가
	EAnimationState AnimationState;
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
	FCharacterState ServerState;
	ACharacter* ServerCharacter;
	TQueue<FCharacterState> ReceivedDataQueue;
	TArray<SOCKET> ClientSockets;
	TMap<SOCKET, ACharacter*> ClientCharacters;
	TMap<SOCKET, FCharacterState> ClientStates;
	TMap<int32, AReplicatedPhysicsBlock*> BlockMap;
	TMap<int32, FTransform> BlockTransforms;
	FCriticalSection ClientSocketsMutex;
	bool bIsRunning = false;
	uint8 playerHeader = 0x00;
	uint8 objectHeader = 0x01;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool InitializeServer(int32 Port);
	void LogAndCleanupSocketError(const TCHAR* ErrorMessage);
	void AcceptClientAsync();
	void InitializeBlocks();
	void SendPlayerData(SOCKET TargetSocket);
	void SendObjectData(int32 BlockID, FTransform NewTransform);
	FCharacterState GetServerCharacterState();
	void ReceiveData(SOCKET ClientSocket);
	void ProcessPlayerData(SOCKET ClientSocket, char* Buffer, int32 BytesReceived);
	void SpawnClientCharacter(SOCKET ClientSocket, const FCharacterState& State);
	void SpawnOrUpdateClientCharacter(SOCKET ClientSocket, const FCharacterState& State);
	void UpdateCharacterState(ACharacter* Character, const FCharacterState& State);
	void UpdateAnimInstanceProperties(UAnimInstance* AnimInstance, const FCharacterState& State);
	void CloseClientSocket(SOCKET ClientSocket);
	void CloseAllClientSockets();
	void CloseServerSocket();

	UPROPERTY()
	bool bIsServer = true;
};
