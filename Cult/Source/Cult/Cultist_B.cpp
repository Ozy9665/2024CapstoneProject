// Fill out your copyright notice in the Description page of Project Settings.


#include "Cultist_B.h"

ACultist_B::ACultist_B()
{
	// Setting
	WalkSpeed = 600.0f;
	Health = 80.0f;
	SpecialAbility = ESpecialAbility::Rolling;
}

void ACultist_B::UseAbility()
{
	UE_LOG(LogTemp, Warning, TEXT("Rolling!"));
}