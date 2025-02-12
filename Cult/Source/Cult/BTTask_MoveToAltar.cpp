// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_MoveToAltar.h"
#include "AIController.h"
#include "CultistCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_MoveToAltar::UBTTask_MoveToAltar()
{
	NodeName = TEXT("Move To Altar");
}

EBTNodeResult::Type UBTTask_MoveToAltar::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)return EBTNodeResult::Failed;

	ACultistCharacter* Cultist = Cast<ACultistCharacter>(AIController->GetPawn());
	if (!Cultist) return EBTNodeResult::Failed;

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard) return EBTNodeResult::Failed;

	FVector AltarLocation = Blackboard->GetValueAsVector("AltarLocation");

	AIController->MoveToLocation(AltarLocation);

	return EBTNodeResult::Succeeded;
}
