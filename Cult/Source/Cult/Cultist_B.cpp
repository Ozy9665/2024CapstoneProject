// Fill out your copyright notice in the Description page of Project Settings.


#include "Cultist_B.h"

ACultist_B::ACultist_B()
{

	// �ŵ� B �⺻ ����
	MovementSpeed = 650.0f;
	Health = 80.0f;

	// �ŵ� B Ư���ɷ� ����
	SpecialAbility = ESpecialAbility::Rolling;
}

void ACultist_B::UseAbility()	// �ŵ� B Ư���ɷ� ����. Rolling ������?
{
	UE_LOG(LogTemp, Warning, TEXT("Use B Ability"));
}