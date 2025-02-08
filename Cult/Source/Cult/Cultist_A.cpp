// Fill out your copyright notice in the Description page of Project Settings.


#include "GameFramework/CharacterMovementComponent.h"
#include "Cultist_A.h"

ACultist_A::ACultist_A()
{
	// Setting
	WalkSpeed = 550.0f;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	SpecialAbility = ESpecialAbility::Vision;
}

void ACultist_A::UseAbility()
{
	UE_LOG(LogTemp, Warning, TEXT("Vision!"));
}