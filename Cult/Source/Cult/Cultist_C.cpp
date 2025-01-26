// Fill out your copyright notice in the Description page of Project Settings.


#include "Cultist_C.h"

ACultist_C::ACultist_C()
{

	// 신도 C 기본 설정
	MovementSpeed = 550.0f;
	Health = 70.0f;
	
	// 신도 C 특수능력 설정
	SpecialAbility = ESpecialAbility::Healing;
}

void ACultist_C::UseAbility()	// 신도 C 특수능력 수행. Healing
{
	UE_LOG(LogTemp, Warning, TEXT("Use C Ability"));
}