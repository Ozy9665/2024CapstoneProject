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
	TMap<FString, FCharacterState> ReceivedCharacterStates;
	TMap<int32, AReplicatedPhysicsBlock*> SyncedBlocks;
	TMap<int32, FTransform> LastReceivedTransform;
	uint8 playerHeader = 0x00;
	uint8 objectHeader = 0x01;
	ACharacter* MyCharacter;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool ConnectToServer(const FString& ServerIP, int32 ServerPort);
	void InitializeBlocks();
	void LogAndCleanupSocketError(const TCHAR* ErrorMessage);
	void ReceiveData();
	void ProcessPlayerData(char* Buffer, int32 BytesReceived);
	void ProcessObjectData(char* Buffer, int32 BytesReceived);
	void SpawnCharacter(const FCharacterState& State);
	void SendPlayerData();
	FCharacterState GetCharacterState(ACharacter* PlayerCharacter);
	void ProcessCharacterUpdates(float DeltaTime);
	void UpdateCharacterState(ACharacter* Character, const FCharacterState& State, float DeltaTime);
	void UpdateAnimInstanceProperties(UAnimInstance* AnimInstance, const FCharacterState& State);
	void ProcessObjectUpdates(float DeltaTime);

};
