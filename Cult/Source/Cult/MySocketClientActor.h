#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <winsock2.h>
#include "MySocketActor.h"
#include "MySocketClientActor.generated.h"

UCLASS()
class CULT_API AMySocketClientActor : public AActor
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
	FCriticalSection ReceivedDataMutex;
	TMap<FString, ACharacter*> SpawnedCharacters;
	TMap<FString, FCultistCharacterState> ReceivedCultistStates;
	TMap<FString, FPoliceCharacterState> ReceivedPoliceStates;
	TMap<int32, AReplicatedPhysicsBlock*> SyncedBlocks;
	TMap<int32, FTransform> LastReceivedTransform;
	TArray<FVector> ImpactLocations;
	UParticleSystem* ImpactParticle;
	uint8 cultistHeader = 0x00;
	uint8 objectHeader = 0x01;
	uint8 policeHeader = 0x10;
	uint8 particleHeader = 0x11;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool ConnectToServer(const FString& ServerIP, int32 ServerPort);
	void InitializeBlocks();
	void LogAndCleanupSocketError(const TCHAR* ErrorMessage);
	void ReceiveData();
	void ProcessPlayerData(char* Buffer, int32 BytesReceived);
	void ProcessObjectData(char* Buffer, int32 BytesReceived);
	void ProcessParticleData(char* Buffer, int32 BytesReceived);
	void SpawnCultistCharacter(const FCultistCharacterState& State);
	void SpawnPoliceCharacter(const FPoliceCharacterState& State);
	void SendPlayerData();
	FCultistCharacterState GetCharacterState(ACharacter* PlayerCharacter);
	void ProcessCharacterUpdates(float DeltaTime);
	void UpdateCultistState(ACharacter* Character, const FCultistCharacterState& State, float DeltaTime);
	void UpdatePoliceState(ACharacter* Character, const FPoliceCharacterState& State, float DeltaTime);
	void UpdateCultistAnimInstanceProperties(UAnimInstance* AnimInstance, const FCultistCharacterState& State);
	void UpdatePoliceAnimInstanceProperties(UAnimInstance* AnimInstance, const FPoliceCharacterState& State);
	void ProcessObjectUpdates(float DeltaTime);
	void SpawnImpactEffect(const FVector& ImpactLocation);

};
