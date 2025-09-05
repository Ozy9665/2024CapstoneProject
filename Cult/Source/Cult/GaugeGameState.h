// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TeamTypeGSBase.h"
#include "GaugeGameState.generated.h"

/**
 * 
 */


UCLASS()
class CULT_API AGaugeGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AGaugeGameState();



protected:
	UPROPERTY(BlueprintReadOnly) FTeamGauge CultistGauge;
	UPROPERTY(BlueprintReadOnly) FTeamGauge PoliceGauge;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gauge")
	TArray<float> LevelThresholds;

	int32 CachedCultistLevel = 0;
	int32 CachedPoliceLevel = 0;



public:

	UFUNCTION() void GetCultistGauge();
	UFUNCTION() void GetPoliceGauge();
};
