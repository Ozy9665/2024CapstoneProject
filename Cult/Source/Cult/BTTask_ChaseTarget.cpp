// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_ChaseTarget.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Character.h"

// Blackboard에 설정된 Target을 추적하기
UBTTask_ChaseTarget::UBTTask_ChaseTarget()
{
	NodeName = "Chase Target";
}

EBTNodeResult::Type UBTTask_ChaseTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;

	ACharacter* AICharacter = Cast<ACharacter>(AIController->GetPawn());
	if (!AICharacter)return EBTNodeResult::Failed;

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject("TargetActor"));

	if (Target)
	{
		AIController->MoveToActor(Target);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}