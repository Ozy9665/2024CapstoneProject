// Fill out your copyright notice in the Description page of Project Settings.

#include "CultPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"

void ACultPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    InputComponent->BindAction("Pause", IE_Pressed, this, &ACultPlayerController::OnPausePressed);
}

void ACultPlayerController::OnPausePressed()
{
    if (!PauseWidget)
    {
        PauseWidget = CreateWidget<UUserWidget>(this, PauseWidgetClass);
        if (PauseWidget)
        {
            PauseWidget->AddToViewport();

            // 입력을 UI Only로
            FInputModeUIOnly InputMode;
            InputMode.SetWidgetToFocus(PauseWidget->TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            SetInputMode(InputMode);

            bShowMouseCursor = true;
        }
        return;
    }

    // 이미 떠있으면 닫기
    PauseWidget->RemoveFromParent();
    PauseWidget = nullptr;

    // 게임 입력 복귀
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);

    bShowMouseCursor = false;
}
