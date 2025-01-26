// Fill out your copyright notice in the Description page of Project Settings.


#include "Cultist_B.h"

ACultist_B::ACultist_B()
{

	// 신도 B 기본 설정
	MovementSpeed = 650.0f;
	Health = 80.0f;

	// 신도 B 특수능력 설정
	SpecialAbility = ESpecialAbility::Rolling;
}

void ACultist_B::UseAbility()	// 신도 B 특수능력 수행. Rolling 구르기?
{
	UE_LOG(LogTemp, Warning, TEXT("Use B Ability"));
}