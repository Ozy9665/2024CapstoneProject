// Fill out your copyright notice in the Description page of Project Settings.


#include "CultGameMode.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "PoliceCharacter.h"

ACultGameMode::ACultGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/Blueprints/BP_PoliceCharacter"));
	if (PlayerPawnClassFinder.Succeeded())
	{
		DefaultPawnClass = PlayerPawnClassFinder.Class;
	}


}

void ACultGameMode::CheckRitualComlete(float CurrentRitualGauge)
{
	if (CurrentRitualGauge >= 100.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Ritual Completed! Ending Game."));
		EndGame();
	}
}

void ACultGameMode::EndGame()
{
	UE_LOG(LogTemp, Warning, TEXT("Game Over."));

	// 게임 재시작
	UGameplayStatics::OpenLevel(this, FName(TEXT("NewWorld")));
}