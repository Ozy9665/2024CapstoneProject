// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CultGameMode.h"
#include "CultPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class CULT_API ACultPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
    virtual void SetupInputComponent() override;

    UFUNCTION()
    void OnPausePressed();

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UUserWidget> PauseWidgetClass;

    UPROPERTY(BlueprintReadWrite, Category = "UI")
    UUserWidget* PauseWidget;
};
