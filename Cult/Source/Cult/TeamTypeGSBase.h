// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TeamTypeGSBase.generated.h"

UENUM(BlueprintType)
enum class ETeam : uint8
{
    Neutral  UMETA(DisplayName = "Neutral"),
    Cultist  UMETA(DisplayName = "Cultist"),
    Police   UMETA(DisplayName = "Police")
};

USTRUCT(BlueprintType)
struct FTeamGauge
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float Value = 0.f; // 0~100
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Level = 0;   // 0~4
};


