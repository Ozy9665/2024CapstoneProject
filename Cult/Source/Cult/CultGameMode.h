// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CultGameMode.generated.h"

UENUM(BlueprintType)
enum class EGAME_END_TYPE : uint8
{
	CultistWin	UMETA(DisplayName = "Cultist Win"),
	PoliceWin	UMETA(DisplayName = "Police Win")
};

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

	

	// 의식 게이지 업데이트
	void CheckRitualComlete(float CurrentRitualGauge);

	// 게임 재시작
	UFUNCTION(BlueprintCallable, Category = "Ending")
	void RestartGame();

	// 게임 종료 처리
	void EndGame(EGAME_END_TYPE EndType);

	// 제단 생성
	UPROPERTY(EditAnywhere, Category = "Altar Spawn")
	TArray<FVector> AltarSpawnLocations;

	UPROPERTY(EditAnywhere, Category = "Altar Spawn")
	TSubclassOf<AActor>AltarClass;
	
	// 페이드인 위젯 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUserWidget> FadeWidgetClass;



	void SpawnAltars();

	UFUNCTION()
	void CheckPoliceVictoryCondition();

	// 게임결과
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UUserWidget> GameResultWidgetClass;

	UUserWidget* GameResultWidget = nullptr;
};
