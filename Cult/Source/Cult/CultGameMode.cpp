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
// #include "MyLegacyCameraShake.h"
// SpawnAltar
#include "Engine/World.h"


ACultGameMode::ACultGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/Blueprints/BP_PoliceCharacter"));
	if (PlayerPawnClassFinder.Succeeded())
	{
		DefaultPawnClass = PlayerPawnClassFinder.Class;
	}


}

void ACultGameMode::BeginPlay()
{
	Super::BeginPlay();
	SpawnAltars();
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
		// PC->ClientStartCameraShake(UMyLegacyCameraShake::StaticClass());
	}

	// 게임 재시작	(3seconds)
	FTimerHandle RestartTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(RestartTimerHandle, this, &ACultGameMode::RestartGame, 3.0f, false);
}

void ACultGameMode::RestartGame()
{
	UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()), false);
}

void ACultGameMode::SpawnAltars()
{
	if (!AltarClass) return;

	int32 Index1 = FMath::RandRange(0, 3);	// 현재는 n 개중 랜덤 조절
	int32 Index2;

	do {
		Index2 = FMath::RandRange(0, 3);
	} while (Index1 == Index2);		// 중복 방지

	// 두 인덱스에 스폰
	FVector SpawnLocation1 = AltarSpawnLocations[Index1];
	FVector SpawnLocation2 = AltarSpawnLocations[Index2];

	FRotator SpawnRotation = FRotator::ZeroRotator;

	AActor* SpawnedAltar1 = GetWorld()->SpawnActor<AActor>(AltarClass, SpawnLocation1, SpawnRotation);
	AActor* SpawnedAltar2 = GetWorld()->SpawnActor<AActor>(AltarClass, SpawnLocation2, SpawnRotation);

	// 스케일
	if (SpawnedAltar1) {
		SpawnedAltar1->SetActorScale3D(FVector(5.0f));
	}
	if (SpawnedAltar2)
	{
		SpawnedAltar2->SetActorScale3D(FVector(5.0f));
	}
}