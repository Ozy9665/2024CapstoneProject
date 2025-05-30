// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "MySocketActor.h"
#include "MyGameInstance.h"
#include <Kismet/GameplayStatics.h>
#include "MySocketPoliceActor.generated.h"

class APoliceCharacter;

UCLASS()
class CULT_API AMySocketPoliceActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMySocketPoliceActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	SOCKET ClientSocket;
	int my_ID = -1;
	APoliceCharacter* MyCharacter;
	FCriticalSection ReceivedDataMutex;
	TMap<int, ACharacter*> SpawnedCharacters;
	TMap<int, FCultistCharacterState> ReceivedCultistStates;
	TMap<int32, FTransform> LastReceivedTransform;
	TArray<FVector> ImpactLocations;
	UMyGameInstance* GI;
	TArray<int32> KeysToRemove;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void SetClientSocket(SOCKET InSocket, int32 RoomNumber);
	void LogAndCleanupSocketError(const TCHAR* ErrorMessage);
	void ReceiveData();
	void ProcessPlayerData(const char* Buffer);
	void ProcessHitData(const char* Buffer);
	void ProcessSkillData(const char* Buffer);
	void ProcessConnection(const char* Buffer);
	void ProcessDisconnection(const char* Buffer);
	void SendPlayerData();
	void SendHitData(FHitPacket hitPacket);
	FPoliceCharacterState GetCharacterState();
	void SpawnCultistCharacter(const unsigned char PlayerID);
	void ProcessCharacterUpdates();
	void UpdateCultistState(ACharacter* Character, const FCultistCharacterState& State);
	void UpdateCultistAnimInstanceProperties(UAnimInstance* AnimInstance, const FCultistCharacterState& State);
	void CheckImpactEffect();
	void SpawnImpactEffect(FHitResult HitResult);
	void SendParticleData(FHitResult HitResult);
	void SendDisconnection();
	void CloseConnection();
	void SafeDestroyCharacter(int PlayerID);
	const TMap<int, ACharacter*>& GetSpawnedCharacters() const;
};
