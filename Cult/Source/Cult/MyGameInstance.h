// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include <NiagaraSystem.h>
#include <winsock2.h>
#include "MyGameInstance.generated.h"

UCLASS()
class CULT_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Role Selection")
	bool bIsCultist = false;

	UPROPERTY(BlueprintReadWrite, Category = "Role Selection")
	bool bIsPolice = false;

	UPROPERTY(BlueprintReadWrite, Category = "Network")
	FString Id;

	UPROPERTY(BlueprintReadWrite, Category = "Network")
	bool canUseId = false;

	UPROPERTY(BlueprintReadWrite, Category = "Network")
	FString Password;

	UPROPERTY(BlueprintReadWrite, Category = "Network")
	FString Verify_Password;

	UPROPERTY(BlueprintReadWrite, Category = "Network")
	int32 PendingRoomNumber = -1;

	SOCKET ClientSocket = INVALID_SOCKET;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	UNiagaraSystem* NGParticleAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
	UNiagaraSystem* MuzzleEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TSubclassOf<class APawn> CultistClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TSubclassOf<class ACharacter> CultistClientClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TSubclassOf<class ACharacter> PoliceClientClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TSubclassOf<class APawn> PoliceClass;

	UPROPERTY(BlueprintReadWrite, Category = "Network")
	TArray<FVector> RutialSpawnLocations;
};
