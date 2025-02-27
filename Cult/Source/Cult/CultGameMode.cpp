// Fill out your copyright notice in the Description page of Project Settings.


#include "CultGameMode.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "PoliceCharacter.h"
// Gamemode - camera
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "MyLegacyCameraShake.h"

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

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC) // 실행
	{
		PC->ClientStartCameraShake(UMyLegacyCameraShake::StaticClass());
	}

	// 게임 재시작
	FTimerHandle RestartTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(RestartTimerHandle, this, &ACultGameMode::RestartGame, 5.0f, false);
}

void ACultGameMode::RestartGame()
{
	UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()), false);
}