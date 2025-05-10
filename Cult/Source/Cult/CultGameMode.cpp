// Fill out your copyright notice in the Description page of Project Settings.


#include "CultGameMode.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "PoliceCharacter.h"
#include "GameFramework/Character.h"
// Gamemode - camera
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "MyLegacyCameraShake.h"
// SpawnAltar
#include "Engine/World.h"
// FadeAnim
#include "FadeWidget.h"
// TextBlock
#include "Components/TextBlock.h"

ACultGameMode::ACultGameMode()
{
	//static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/Blueprints/BP_PoliceCharacter"));
	//if (PlayerPawnClassFinder.Succeeded())
	//{
	//	DefaultPawnClass = PlayerPawnClassFinder.Class;
	//}


}

void ACultGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	// 현재 레벨
	FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (CurrentLevelName == "NewWorld")
	{
		SpawnAltars();
	}

	if (FadeWidgetClass)
	{
		UFadeWidget* FadeWidget = CreateWidget<UFadeWidget>(GetWorld(), FadeWidgetClass);
		if (FadeWidget)
		{
			FadeWidget->AddToViewport();
			FadeWidget->PlayFadeAnim();
		}
	}
}

void ACultGameMode::CheckRitualComlete(float CurrentRitualGauge)
{
	if (CurrentRitualGauge >= 100.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Ritual Completed! Ending Game."));
		EndGame(EGAME_END_TYPE::CultistWin);
	}
}

void ACultGameMode::EndGame(EGAME_END_TYPE EndType)
{
	UE_LOG(LogTemp, Warning, TEXT("Game Over."));

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC) // 실행
	{
		PC->bShowMouseCursor = true;
		PC->SetInputMode(FInputModeUIOnly());

		if (GameResultWidgetClass)
		{
			GameResultWidget = CreateWidget<UUserWidget>(PC, GameResultWidgetClass);
			if (GameResultWidget)
			{
				GameResultWidget->AddToViewport();

				FString ResultText;
				UTextBlock* ResultTextBlock = Cast<UTextBlock>(GameResultWidget->GetWidgetFromName(TEXT("TextBlock_ResultText")));

				switch (EndType)
				{
				case EGAME_END_TYPE::CultistWin:
					ResultText = TEXT("Cultist Win");
					if (ResultTextBlock)
					{
						ResultTextBlock->SetText(FText::FromString(ResultText));
					}
					PC->ClientStartCameraShake(UMyLegacyCameraShake::StaticClass());
					break;
				case EGAME_END_TYPE::PoliceWin:
					ResultText = TEXT("Police Win");
					if (ResultTextBlock)
					{
						ResultTextBlock->SetText(FText::FromString(ResultText));
					}
					break;
				}

				
			}
		}
	}

	// 게임 재시작	(3seconds)
	//FTimerHandle RestartTimerHandle;
	//GetWorld()->GetTimerManager().SetTimer(RestartTimerHandle, this, &ACultGameMode::RestartGame, 5.0f, false);
}

void ACultGameMode::RestartGame()
{
	UGameplayStatics::OpenLevel(this, FName("CharacterUI"));
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

void ACultGameMode::CheckPoliceVictoryCondition()
{
	TArray<AActor*> Cultists;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACultistCharacter::StaticClass(), Cultists);
	for (AActor* Actor : Cultists)
	{
		ACultistCharacter* Cultist = Cast<ACultistCharacter>(Actor);
		if (Cultist && !Cultist->IsInactive())
		{
			// 해당 신도가 죽거나 감금당하면 true. 부정이므로 죽지도 감금당하지도 않았을 시 return
			return;
		}
	}
	// return 안됐음 -> 생존자 없음
	UE_LOG(LogTemp, Warning, TEXT("All Cultist Dead or Confined. PoliceWin"));
	EndGame(EGAME_END_TYPE::PoliceWin);
}