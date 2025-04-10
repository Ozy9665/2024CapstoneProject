// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CharacterSelectGameMode.generated.h"

UCLASS()
class CULT_API ACharacterSelectGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, Category = "Character Selection")
	bool bIsCultist{ false };

	UPROPERTY(BlueprintReadWrite, Category = "Character Selection")
	bool bIsPolice{ false };

	UFUNCTION(BlueprintCallable, Category = "Character Selection")
	void SetCultistSelected();

	UFUNCTION(BlueprintCallable, Category = "Character Selection")
	void SetPoliceSelected();
};
