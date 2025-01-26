// Fill out your copyright notice in the Description page of Project Settings.


#include "PoliceCharacter.h"
#include "CultistCharacter.h"

APoliceCharacter::APoliceCharacter()
{
	CurrentWeapon = EWeaponType::Baton;
}

void APoliceCharacter::BeginPlay()
{
	Super::BeginPlay();
}

// 공격
void APoliceCharacter::Attack()
{
	if (CurrentWeapon == EWeaponType::Baton)			// 근접공격
	{
		UE_LOG(LogTemp, Warning, TEXT("MeleeAttack"));
	}
	else if (CurrentWeapon == EWeaponType::Pistol)							// 권총
	{
		UE_LOG(LogTemp, Warning, TEXT("Fire Pistol"));
	}
	else if (CurrentWeapon == EWeaponType::Taser)							// 테이저건
	{
		UE_LOG(LogTemp, Warning, TEXT("Fire Taser"));
	}
}

// 체포
void APoliceCharacter::ArrestTarget(ABaseCharacter* Target)
{
	if (Target && Target->CharacterState == ECharacterState::Stunned)	// 기절->체포로 상태 변경
	{
		Target->SetCharacterState(ECharacterState::Captured);
		UE_LOG(LogTemp, Warning, TEXT("Target Captured"));
	}
}