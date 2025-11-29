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

	void SetClientSocket(SOCKET, int32);
	void LogAndCleanupSocketError(const TCHAR*);
	void ReceiveData();
	void ProcessPlayerData(const char*);
	void ProcessHitData(const char*);
	void ProcessTreeData(const char*);
	void ProcessCrowSpawnData(const char*);
	void ProcessCrowData(const char*);
	void ProcessCrowDisable(const char* Buffer);
	void ProcessConnection(const char*);
	void ProcessDisconnection(const char*);
	void SendPlayerData();
	void SendDogData();
	void SendHitData(EWeaponType);
	FPoliceCharacterState GetCharacterState();
	Dog GetDog();
	void SpawnCultistCharacter(const unsigned char);
	void ProcessCharacterUpdates();
	void UpdateCultistState(ACharacter*, const FCultistCharacterState&);
	void UpdateCultistAnimInstanceProperties(UAnimInstance*, const FCultistCharacterState&);
	void CheckImpactEffect();
	void SpawnImpactEffect(FHitResult);
	void SendParticleData(FHitResult);
	void HideCharacter(int, bool);
	UFUNCTION(BlueprintCallable)
	void SendQuit();
	void SendDisconnection();
	void CloseConnection();
	void SafeDestroyCharacter(int);
	const TMap<int, ACharacter*>& GetSpawnedCharacters() const;
};
