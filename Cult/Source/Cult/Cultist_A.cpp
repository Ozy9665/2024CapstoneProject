// Fill out your copyright notice in the Description page of Project Settings.

#include "Cultist_A.h"
#include "GameFramework/CharacterMovementComponent.h"

ACultist_A::ACultist_A()
{
	// Setting
	WalkSpeed = 600.0f;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	SpecialAbility = ESpecialAbility::Vision;
}

void ACultist_A::UseAbility()
{
	UE_LOG(LogTemp, Warning, TEXT("Vision!"));
}