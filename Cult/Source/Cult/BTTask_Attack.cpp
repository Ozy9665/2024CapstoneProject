// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_Attack.h"
#include "AIController.h"
#include "AI_PoliceAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "PoliceCharacter.h"

UBTTask_Attack::UBTTask_Attack()
{
	NodeName = "Attack Target";
}

EBTNodeResult::Type UBTTask_Attack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAI_PoliceAIController* AIController = Cast<AAI_PoliceAIController>(OwnerComp.GetAIOwner());
	if (!AIController)return EBTNodeResult::Failed;

	APoliceCharacter* PoliceAI = Cast<APoliceCharacter>(AIController->GetPawn());
	if (!PoliceAI) return EBTNodeResult::Failed;

	if (PoliceAI->bIsAttacking)
	{
		return EBTNodeResult::Failed;
	}
	PoliceAI->StartAttack();

	bNotifyTick = true;

	return EBTNodeResult::InProgress;
}

void UBTTask_Attack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAI_PoliceAIController* AIController = Cast<AAI_PoliceAIController>(OwnerComp.GetAIOwner());
	if (!AIController)return ;

	APoliceCharacter* PoliceAI = Cast<APoliceCharacter>(AIController->GetPawn());
	if (!PoliceAI) return ;

	if (!PoliceAI->bIsAttacking)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}