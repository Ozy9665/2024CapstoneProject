// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Kismet/GameplayStatics.h>
#include "MySocketActor.h"
#include "MyGameInstance.h"
#include "CultistCharacter.h"
#include "MySocketCultistActor.generated.h"

class ACultistCharacter;

UCLASS()
class CULT_API AMySocketCultistActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMySocketCultistActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	SOCKET ClientSocket;
	int my_ID = -1;
	ACultistCharacter* MyCharacter;
	FCriticalSection CultistDataMutex;
	FCriticalSection PoliceDataMutex;
	TMap<int, ACharacter*> SpawnedCharacters;
	TMap<int, FCultistCharacterState> ReceivedCultistStates;
	TMap<int, FPoliceCharacterState> ReceivedPoliceStates;
	TMap<int32, FTransform> LastReceivedTransform;
	TArray<FImpactPacket> Particles;
	UMyGameInstance* GI;
	TArray<int32> KeysToRemove;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	void SetClientSocket(SOCKET InSocket, int32 RoomNumber);
	void LogAndCleanupSocketError(const TCHAR* ErrorMessage);
	void ReceiveData();
	void ProcessCultistData(const char* Buffer);
	void ProcessPoliceData(const char* Buffer);
	void ProcessHitData(const char* Buffer);
	void ProcessConnection(const char* Buffer);
	void ProcessDisconnection(const char* Buffer);
	void SendSkill(FVector, FRotator);
	void SendDisable();
	void SendPlayerData();
	FCultistCharacterState GetCharacterState();
	void ProcessCharacterUpdates();
	void UpdateCultistState(ACharacter* Character, const FCultistCharacterState& State);
	void UpdateCultistAnimInstanceProperties(UAnimInstance* AnimInstance, const FCultistCharacterState& State);
	void SpawnCultistCharacter(const unsigned char PlayerID);
	void UpdatePoliceState(ACharacter* Character, const FPoliceCharacterState& State);
	void UpdatePoliceAnimInstanceProperties(UAnimInstance* AnimInstance, const FPoliceCharacterState& State);
	void SpawnPoliceCharacter(const unsigned char PlayerID);
	void SpawnPoliceAICharacter(const unsigned char PlayerID);
	void ProcessParticleData(char* Buffer);
	void SpawnImpactEffect(const FImpactPacket& ReceivedImpact);
	void SendDisconnection();
	void CloseConnection();
	void SafeDestroyCharacter(int PlayerID);
};
