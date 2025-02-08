// Fill out your copyright notice in the Description page of Project Settings.


#include "CultGameMode.h"
#include "UObject/ConstructorHelpers.h"
#include "PoliceCharacter.h"

ACultGameMode::ACultGameMode()
{
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/Blueprints/BP_PoliceCharacter"));
	if (PlayerPawnClassFinder.Succeeded())
	{
		DefaultPawnClass = PlayerPawnClassFinder.Class;
	}
}