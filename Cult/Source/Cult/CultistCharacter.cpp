// Fill out your copyright notice in the Description page of Project Settings.


#include "CultistCharacter.h"

ACultistCharacter::ACultistCharacter()
{
	// 능력 지정
}

void ACultistCharacter::BeginPlay()
{
	Super::BeginPlay();
}

// 의식 수행
void ACultistCharacter::PerformRitual()
{
	UE_LOG(LogTemp, Warning, TEXT("Perform ritual..."));
}