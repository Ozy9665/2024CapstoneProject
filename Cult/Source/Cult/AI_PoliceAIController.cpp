// Fill out your copyright notice in the Description page of Project Settings.


#include "AI_PoliceAIController.h"



void AAI_PoliceAIController::BeginPlay()
{
	Super::BeginPlay();

	/*if (AIBehaviorTree)
	{
		RunBehaviorTree(AIBehaviorTree);
	}*/


}

void AAI_PoliceAIController::StartChase(AActor* Target)
{
	if (Blackboard)
	{
		Blackboard->SetValueAsObject(TEXT("Target"), Target);
	}
}

void AAI_PoliceAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	
	if (UseBlackboard(BlackboardAsset, BlackboardComponent))
	{
		RunBehaviorTree(AIBehaviorTree);
	}
 }