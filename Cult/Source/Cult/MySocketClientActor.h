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
	int my_ID = -1;
	FCriticalSection ReceivedDataMutex;
	TMap<int, ACharacter*> SpawnedCharacters;
	TMap<int, FCultistCharacterState> ReceivedCultistStates;
	TMap<int, FPoliceCharacterState> ReceivedPoliceStates;
	TMap<int32, AReplicatedPhysicsBlock*> SyncedBlocks;
	TMap<int32, FTransform> LastReceivedTransform;
	TArray<FVector> ImpactLocations;
	static const int cultistHeader = 0x00;
	static const int objectHeader = 0x01;
	static const int policeHeader = 0x02;
	static const int particleHeader = 0x03;
	static const int connectionHeader = 0x10;
	static const int DisconnectionHeader = 0x11;
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UParticleSystem* ImpactParticle;
	FCultistCharacterState DummyState{ -1, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.0f };

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool ConnectToServer(const FString& ServerIP, int32 ServerPort);
	//void InitializeBlocks();
	void LogAndCleanupSocketError(const TCHAR* ErrorMessage);
	void ReceiveData();
	void ProcessPlayerData(char* Buffer, int32 BytesReceived);
	//void ProcessObjectData(char* Buffer, int32 BytesReceived);
	//void ProcessParticleData(char* Buffer, int32 BytesReceived);
	void ProcessConnection(char* Buffer, int32 BytesReceived);
	void ProcessDisconnection(char* Buffer, int32 BytesReceived);
	void SpawnCultistCharacter(const char* Buffer);
	//void SpawnPoliceCharacter(const FPoliceCharacterState& State);
	void SendPlayerData();
	FCultistCharacterState GetCharacterState(ACharacter* PlayerCharacter);
	void ProcessCharacterUpdates(float DeltaTime);
	void UpdateCultistState(ACharacter* Character, const FCultistCharacterState& State, float DeltaTime);
	//void UpdatePoliceState(ACharacter* Character, const FPoliceCharacterState& State, float DeltaTime);
	void UpdateCultistAnimInstanceProperties(UAnimInstance* AnimInstance, const FCultistCharacterState& State);
	//void UpdatePoliceAnimInstanceProperties(UAnimInstance* AnimInstance, const FPoliceCharacterState& State);
	//void ProcessObjectUpdates(float DeltaTime);
	//void SpawnImpactEffect(const FVector& ImpactLocation);
	void CloseConnection();
};
