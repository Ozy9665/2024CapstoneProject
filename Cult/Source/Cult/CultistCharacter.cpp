// Fill out your copyright notice in the Description page of Project Settings.


#include "CultistCharacter.h"

ACultistCharacter::ACultistCharacter()
{
	// �ɷ� ����
}

void ACultistCharacter::BeginPlay()
{
	Super::BeginPlay();
}

// �ǽ� ����
void ACultistCharacter::PerformRitual()
{
	UE_LOG(LogTemp, Warning, TEXT("Perform ritual..."));
}