// Fill out your copyright notice in the Description page of Project Settings.


#include "Cultist_A.h"

ACultist_A::ACultist_A()
{

	// 신도 A 기본 설정
	MovementSpeed = 600.0f;
	Health = 100.0f;

	// 신도 A 특수능력 설정
	SpecialAbility = ESpecialAbility::Vision;

}

void ACultist_A::UseAbility()	// 신도 A 특수능력 수행. Vision
{
	UE_LOG(LogTemp, Warning, TEXT("Use A Ability"));
}