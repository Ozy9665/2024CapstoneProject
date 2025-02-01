// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Containers/Queue.h"
#include "ClientCharacter.h"
#include <winsock2.h>
#include "MySocketActor.generated.h"

UENUM(BlueprintType)
enum class EAnimationState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Running UMETA(DisplayName = "Run"),
	Jumping UMETA(DisplayName = "Jump")
};

USTRUCT(BlueprintType)
struct FCharacterState
{
	GENERATED_BODY()
	int32 PlayerID;
	// ��ġ
	float PositionX;
	float PositionY;
	float PositionZ;
	// ȸ��
	float RotationPitch;
	float RotationYaw;
	float RotationRoll;
	// �ӵ�
	float VelocityX;
	float VelocityY;
	float VelocityZ;
	float GroundSpeed;
	bool bIsFalling;
	// �ִϸ��̼� ���� �ʵ� �߰�
	EAnimationState AnimationState;
};

UCLASS()
class PROJECT2_API AMySocketActor : public AActor
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
	FCriticalSection ClientSocketsMutex;
	bool bIsRunning = false;
	SOCKET BroadcastSocket = INVALID_SOCKET;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool InitializeServer(int32 Port);  // ���� �ʱ�ȭ �Լ�
	void AcceptClientAsync();
	void sendData(SOCKET TargetSocket);
	void ReceiveData(SOCKET ClientSocket);
	void SpawnClientCharacter(SOCKET ClientSocket, const FCharacterState& State);
	void SpawnOrUpdateClientCharacter(SOCKET ClientSocket, const FCharacterState& State);
	void CloseClientSocket(SOCKET ClientSocket);
	bool InitializeUDPSocket(int32 Port);
	void BroadcastData();
};
