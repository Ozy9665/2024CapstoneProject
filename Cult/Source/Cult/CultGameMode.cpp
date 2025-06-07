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
#include "MyGameInstance.h"

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
	if (CurrentLevelName == "NewMap_LandMass")
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnAltars Called"));
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
		//if (MySocketCultistActor)
		//{
		//	MySocketCultistActor->SendDisable();
		//}
	}
}

void ACultGameMode::EndGame(EGAME_END_TYPE EndType)
{
	UE_LOG(LogTemp, Warning, TEXT("Game Over."));

	//APlayerController* PC = GetWorld()->GetFirstPlayerController();
	//if (PC) // 실행
	//{
	//	PC->bShowMouseCursor = true;
	//	PC->SetInputMode(FInputModeUIOnly());

	//	if (GameResultWidgetClass)
	//	{
	//		GameResultWidget = CreateWidget<UUserWidget>(PC, GameResultWidgetClass);
	//		if (GameResultWidget)
	//		{
	//			GameResultWidget->AddToViewport();

	//			FString ResultText;
	//			UTextBlock* ResultTextBlock = Cast<UTextBlock>(GameResultWidget->GetWidgetFromName(TEXT("TextBlock_ResultText")));

	//			switch (EndType)
	//			{
	//			case EGAME_END_TYPE::CultistWin:
	//				ResultText = TEXT("Cultist Win");
	//				if (ResultTextBlock)
	//				{
	//					ResultTextBlock->SetText(FText::FromString(ResultText));
	//				}
	//				PC->ClientStartCameraShake(UMyLegacyCameraShake::StaticClass());
	//				break;
	//			case EGAME_END_TYPE::PoliceWin:
	//				ResultText = TEXT("Police Win");
	//				if (ResultTextBlock)
	//				{
	//					ResultTextBlock->SetText(FText::FromString(ResultText));
	//				}
	//				break;
	//			}

	//			
	//		}
	//	}
	//}

	// 게임 재시작	(3seconds)
	//FTimerHandle RestartTimerHandle;
	//GetWorld()->GetTimerManager().SetTimer(RestartTimerHandle, this, &ACultGameMode::RestartGame, 5.0f, false);
}

void ACultGameMode::RestartGame()
{
	UGameplayStatics::OpenLevel(this, FName("CharaterUI"));
}

void ACultGameMode::SpawnAltars()
{
	if (!AltarClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AltarClass is nullptr"));
		return;
	}

	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	if (!GI || GI->RutialSpawnLocations.Num() <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("GI->RutialSpawnLocations is empty or GI null"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("SpawnAltars() using GI: %d locations"), GI->RutialSpawnLocations.Num());

	for (int32 i = 0; i < GI->RutialSpawnLocations.Num(); ++i)
	{	
		AActor* SpawnedAltar = GetWorld()->SpawnActor<AActor>(AltarClass, GI->RutialSpawnLocations[i], FRotator::ZeroRotator);

		if (SpawnedAltar)
		{
			SpawnedAltar->SetActorScale3D(FVector(5.0f));
			UE_LOG(LogTemp, Warning, TEXT("Spawned Altar[%d] at %s"), i, *GI->RutialSpawnLocations[i].ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to spawn Altar[%d] at %s"), i, *GI->RutialSpawnLocations[i].ToString());
		}
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
	//EndGame(EGAME_END_TYPE::PoliceWin);
}