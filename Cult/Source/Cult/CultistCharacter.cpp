// Fill out your copyright notice in the Description page of Project Settings.


#include "CultistCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/InputComponent.h"


ACultistCharacter::ACultistCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	WalkSpeed = 600.0f;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	bIsPerformingRitual = false;
	RitualProgress = 0.0f;
	RitualSpeed = 10.0f; // 초당 증가속도
}

void ACultistCharacter::BeginPlay()
{
	Super::BeginPlay();
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

void ACultistCharacter::StartRitual()
{
	if (bIsPerformingRitual || !CurrentAltar) return;

	bIsPerformingRitual = true;
	RitualProgress = 0.0f;

	UE_LOG(LogTemp, Warning, TEXT("Ritual Started"));

	GetCharacterMovement()->DisableMovement();

	// Animation
}

void ACultistCharacter::StopRitual()
{
	if (!bIsPerformingRitual)return;

	bIsPerformingRitual = false;


	UE_LOG(LogTemp, Warning, TEXT("Ritual Stopped"));

	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ACultistCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsPerformingRitual)
	{
		RitualProgress += RitualSpeed * DeltaTime;

		if (RitualProgress >= 100.0f) // 의식이 완료되면 중지
		{
			StopRitual();
			UE_LOG(LogTemp, Warning, TEXT("Ritual Completed!"));
		}
	}
}