// Fill out your copyright notice in the Description page of Project Settings.


#include "PoliceCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

APoliceCharacter::APoliceCharacter()
{
	CurrentWeapon = EWeaponType::Baton;	
	WalkSpeed = 650.0f;	// more faster than cultist
}

void APoliceCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void APoliceCharacter::WeaponAttack()
{
	if (CurrentWeapon == EWeaponType::Baton)
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack with Baton"));
	}
	else if (CurrentWeapon == EWeaponType::Pistol)
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack with Pistol"));
	}
	if (CurrentWeapon == EWeaponType::Taser)
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack with Taser"));
	}
}