// Fill out your copyright notice in the Description page of Project Settings.


#include "Cultist_C.h"

ACultist_C::ACultist_C()
{

	// �ŵ� C �⺻ ����
	MovementSpeed = 550.0f;
	Health = 70.0f;
	
	// �ŵ� C Ư���ɷ� ����
	SpecialAbility = ESpecialAbility::Healing;
}

void ACultist_C::UseAbility()	// �ŵ� C Ư���ɷ� ����. Healing
{
	UE_LOG(LogTemp, Warning, TEXT("Use C Ability"));
}