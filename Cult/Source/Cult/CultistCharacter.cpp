// Fill out your copyright notice in the Description page of Project Settings.


#include "CultistCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"

ACultistCharacter::ACultistCharacter()
{
	WalkSpeed = 600.0f;

	PrimaryActorTick.bCanEverTick = true;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void ACultistCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ACultistCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ACultistCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &ACultistCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveForward", this, &ACultistCharacter::MoveRight);
}

void ACultistCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void ACultistCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void ACultistCharacter::PerformRitual()
{
	UE_LOG(LogTemp, Warning, TEXT("Performing Ritual..."));
}