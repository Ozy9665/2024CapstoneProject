// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CultGameMode.generated.h"

/**
 * 
 */
UCLASS()
class CULT_API ACultGameMode : public AGameModeBase
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:
	ACultGameMode();

	

	// �ǽ� ������ ������Ʈ
	void CheckRitualComlete(float CurrentRitualGauge);

	// ���� �����
	void RestartGame();

	// ���� ���� ó��
	void EndGame();

	// ���� ����
	UPROPERTY(EditAnywhere, Category = "Altar Spawn")
	TArray<FVector> AltarSpawnLocations;

	UPROPERTY(EditAnywhere, Category = "Altar Spawn")
	TSubclassOf<AActor>AltarClass;
	void SpawnAltars();
};
