// Fill out your copyright notice in the Description page of Project Settings.


#include "PoliceAICharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "BehaviorTree/BehaviorTree.h"

APoliceAICharacter::APoliceAICharacter(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

	PrimaryActorTick.bCanEverTick = true;

	// 시야 감지
	AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));


}

void APoliceAICharacter::BeginPlay()
{
	Super::BeginPlay();
}

void APoliceAICharacter::ChaseTarget(AActor* Target)
{
	UE_LOG(LogTemp, Warning, TEXT("Chasing Target: %s"), *Target->GetName());
}

void APoliceAICharacter::AttackTarget()
{
	UE_LOG(LogTemp, Warning, TEXT("Attacking Target"));
}