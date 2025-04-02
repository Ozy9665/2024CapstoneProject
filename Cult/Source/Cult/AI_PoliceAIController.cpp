// Fill out your copyright notice in the Description page of Project Settings.


#include "AI_PoliceAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

void AAI_PoliceAIController::BeginPlay()
{
	Super::BeginPlay();

	if (AIBehaviorTree)
	{
		RunBehaviorTree(AIBehaviorTree);
	}
}

void AAI_PoliceAIController::StartChase(AActor* Target)
{
	if (Blackboard)
	{
		Blackboard->SetValueAsObject(TEXT("Target"), Target);
	}
}
