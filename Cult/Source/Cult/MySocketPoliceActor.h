// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MySocketActor.h"
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
	FCultistCharacterState DummyState{ -1, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.0f };
	TMap<int, FCultistCharacterState> ReceivedCultistStates;
	TMap<int32, AReplicatedPhysicsBlock*> SyncedBlocks;
	TMap<int32, FTransform> LastReceivedTransform;
	static const int cultistHeader = 0x00;
	static const int objectHeader = 0x01;
	static const int policeHeader = 0x02;
	static const int particleHeader = 0x03;
	static const int connectionHeader = 0x10;
	static const int DisconnectionHeader = 0x11;
	TArray<FVector> ImpactLocations;
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UParticleSystem* ImpactParticle;

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
	void ProcessCharacterUpdates(float DeltaTime);
	void UpdateCultistState(ACharacter* Character, const FCultistCharacterState& State, float DeltaTime);
	void UpdateCultistAnimInstanceProperties(UAnimInstance* AnimInstance, const FCultistCharacterState& State);
	void CloseConnection();
	void SafeDestroyCharacter(int PlayerID);
};
