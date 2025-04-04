// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_Attack.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "PoliceCharacter.h"

UBTTask_Attack::UBTTask_Attack()
{
	NodeName = "Attack Target";
}

EBTNodeResult::Type UBTTask_Attack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)return EBTNodeResult::Failed;

	APoliceCharacter* PoliceAI = Cast<APoliceCharacter>(AIController->GetPawn());
	if (!PoliceAI) return EBTNodeResult::Failed;

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject("Target"));

	if (Target)
	{
		PoliceAI->StartAttack();  
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}
