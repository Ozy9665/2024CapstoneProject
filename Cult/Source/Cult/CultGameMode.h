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

	

	// �ǽ� ������ ������Ʈ
	void CheckRitualComlete(float CurrentRitualGauge);

	// ���� �����
	UFUNCTION(BlueprintCallable, Category = "Ending")
	void RestartGame();

	// ���� ���� ó��
	void EndGame(EGAME_END_TYPE EndType);

	// ���� ����
	UPROPERTY(EditAnywhere, Category = "Altar Spawn")
	TArray<FVector> AltarSpawnLocations;

	UPROPERTY(EditAnywhere, Category = "Altar Spawn")
	TSubclassOf<AActor>AltarClass;
	
	// ���̵��� ���� Ŭ����
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUserWidget> FadeWidgetClass;



	void SpawnAltars();

	UFUNCTION()
	void CheckPoliceVictoryCondition();

	// ���Ӱ��
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UUserWidget> GameResultWidgetClass;

	UUserWidget* GameResultWidget = nullptr;
};
