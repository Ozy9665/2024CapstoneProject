// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "MySocketActor.h"
#include "PoliceCharacter.h"
#include <Kismet/GameplayStatics.h>
#include "MySocketPoliceActor.generated.h"

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
	FCultistCharacterState CultistDummyState{ -1, 110, -1100,  2770, 0, 90, 0 };
	TMap<int, FCultistCharacterState> ReceivedCultistStates;
	TMap<int32, FTransform> LastReceivedTransform;
	TArray<FVector> ImpactLocations;
	UNiagaraSystem* NG_ImpactParticle;
	UNiagaraSystem* MuzzleImpactParticle;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void SetClientSocket(SOCKET InSocket);
	void LogAndCleanupSocketError(const TCHAR* ErrorMessage);
	void ReceiveData();
	void ProcessPlayerData(char* Buffer, int32 BytesReceived);
	void ProcessConnection(char* Buffer, int32 BytesReceived);
	void ProcessDisconnection(char* Buffer, int32 BytesReceived);
	void SendPlayerData();
	FPoliceCharacterState GetCharacterState();
	void SpawnCultistCharacter(const char* Buffer);
	void ProcessCharacterUpdates();
	void UpdateCultistState(ACharacter* Character, const FCultistCharacterState& State);
	void UpdateCultistAnimInstanceProperties(UAnimInstance* AnimInstance, const FCultistCharacterState& State);
	void CheckImpactEffect();
	void SpawnImpactEffect(FHitResult HitResult);
	void CloseConnection();
	void SafeDestroyCharacter(int PlayerID);
};
