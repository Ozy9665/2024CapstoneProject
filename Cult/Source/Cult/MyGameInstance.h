// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MyGameInstance.generated.h"

UCLASS()
class CULT_API UMyGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Role Selection")
	bool bIsCultist = false;

	UPROPERTY(BlueprintReadWrite, Category = "Role Selection")
	bool bIsPolice = false;
};
