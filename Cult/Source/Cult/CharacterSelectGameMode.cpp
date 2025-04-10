// Fill out your copyright notice in the Description page of Project Settings.


#include "CharacterSelectGameMode.h"

void ACharacterSelectGameMode::SetCultistSelected()
{
	bIsCultist = true;
	bIsPolice = false;
}

void ACharacterSelectGameMode::SetPoliceSelected()
{
	bIsCultist = false;
	bIsPolice = true;
}