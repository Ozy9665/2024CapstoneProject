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

public:
	ACultGameMode();

	// 의식 게이지 업데이트
	void CheckRitualComlete(float CurrentRitualGauge);

	// 게임 종료 처리
	void EndGame();
};
