// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Kismet/GameplayStatics.h>
#include "MySocketActor.h"
#include "MyGameInstance.h"
#include "Animation/AnimInstance.h"
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
	TWeakObjectPtr<UAnimInstance> BoundAnimInstance;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	void SetClientSocket(SOCKET, int32);
	void LogAndCleanupSocketError(const TCHAR*);
	void ReceiveData();
	void ProcessCultistData(const char*);
	void ProcessSkillData(const char*);
	void ProcessPoliceData(const char*);
	void ProcessDogData(const char*);
	void ProcessHitData(const char*);
	void ProcessConnection(const char*);
	void ProcessDisconnection(const char*);
	void SendSkill(FVector, FRotator, int32);
	void SendDisable();
	void SendPlayerData();
	FCultistCharacterState GetCharacterState();
	void ProcessCharacterUpdates();
	void UpdateCultistState(ACharacter*, const FCultistCharacterState&);
	void UpdateCultistAnimInstanceProperties(UAnimInstance*, const FCultistCharacterState&);
	void SpawnCultistCharacter(const unsigned char);
	void UpdatePoliceState(ACharacter* Character, const FPoliceCharacterState&);
	void UpdatePoliceAnimInstanceProperties(UAnimInstance*, const FPoliceCharacterState&);
	void SpawnPoliceCharacter(const unsigned char);
	void SpawnPoliceAICharacter(const unsigned char);
	void ProcessParticleData(const char*);
	void SpawnImpactEffect(const FImpactPacket&);
	void HideCharacter(int, bool);
	void SendTryHeal();
	void SendEndHeal();
	void ProcessDoHeal(const char*);
	void SendStartRitual(uint8_t);
	void SendRitualSkillCheck(uint8_t, uint8_t);
	void SendEndRitual(uint8_t);
	void ProcessRitualData(const char*);
	void ProcessRitualEnd(const char*);
	void SendDisconnection();
	void CloseConnection();
	void SafeDestroyCharacter(int);

	UFUNCTION()
	void HandleMontageSitNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload);
	UFUNCTION()
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

};
