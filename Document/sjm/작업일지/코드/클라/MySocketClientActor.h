// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <winsock2.h>
#include "MySocketClientActor.generated.h"

UENUM(BlueprintType)
enum class EAnimationState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Run UMETA(DisplayName = "Run"),
	Jump UMETA(DisplayName = "Jump")
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
	float GroundSpeed;
	bool bIsFalling;
	// 애니메이션 상태 필드 추가
	EAnimationState AnimationState;
};

UCLASS()
class PROJECT1_API AMySocketClientActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMySocketClientActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	SOCKET ClientSocket;
	TMap<FString, ACharacter*> SpawnedCharacters;
	TMap<FString, FCharacterState> ReceivedCharacterStates;
	FCriticalSection ReceivedDataMutex;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool ConnectToServer(const FString& ServerIP, int32 ServerPort);
	void ReceiveData();
	void SpawnCharacter(const FCharacterState& State);
	void SendData();
	void ProcessCharacterUpdates(float DeltaTime);
};
