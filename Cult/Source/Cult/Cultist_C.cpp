// Fill out your copyright notice in the Description page of Project Settings.


#include "Cultist_C.h"

ACultist_C::ACultist_C()
{
	// Setting
	WalkSpeed = 530.0f;
	Health = 70.0f;
	SpecialAbility = ESpecialAbility::Healing;
}

void ACultist_C::UseAbility()
{
	UE_LOG(LogTemp, Warning, TEXT("Healing.."));
}