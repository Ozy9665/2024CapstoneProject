// Fill out your copyright notice in the Description page of Project Settings.


#include "CultistCharacter.h"

ACultistCharacter::ACultistCharacter()
{
	WalkSpeed = 600.0f;
}

void ACultistCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ACultistCharacter::PerformRitual()
{
	UE_LOG(LogTemp, Warning, TEXT("Performing Ritual..."));
}