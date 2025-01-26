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

// ����
void APoliceCharacter::Attack()
{
	if (CurrentWeapon == EWeaponType::Baton)			// ��������
	{
		UE_LOG(LogTemp, Warning, TEXT("MeleeAttack"));
	}
	else if (CurrentWeapon == EWeaponType::Pistol)							// ����
	{
		UE_LOG(LogTemp, Warning, TEXT("Fire Pistol"));
	}
	else if (CurrentWeapon == EWeaponType::Taser)							// ��������
	{
		UE_LOG(LogTemp, Warning, TEXT("Fire Taser"));
	}
}

// ü��
void APoliceCharacter::ArrestTarget(ABaseCharacter* Target)
{
	if (Target && Target->CharacterState == ECharacterState::Stunned)	// ����->ü���� ���� ����
	{
		Target->SetCharacterState(ECharacterState::Captured);
		UE_LOG(LogTemp, Warning, TEXT("Target Captured"));
	}
}