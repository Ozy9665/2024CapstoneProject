// Fill out your copyright notice in the Description page of Project Settings.


#include "Cultist_A.h"

ACultist_A::ACultist_A()
{

	// �ŵ� A �⺻ ����
	MovementSpeed = 600.0f;
	Health = 100.0f;

	// �ŵ� A Ư���ɷ� ����
	SpecialAbility = ESpecialAbility::Vision;

}

void ACultist_A::UseAbility()	// �ŵ� A Ư���ɷ� ����. Vision
{
	UE_LOG(LogTemp, Warning, TEXT("Use A Ability"));
}