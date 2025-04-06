// Fill out your copyright notice in the Description page of Project Settings.


#include "BTDecorator_IsTargetNear.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/Character.h"

UBTDecorator_IsTargetNear::UBTDecorator_IsTargetNear()
{
	NodeName = "Is Target Near";
}

bool UBTDecorator_IsTargetNear::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject("TargetActor"));
	ACharacter* AICharacter = Cast<ACharacter>(OwnerComp.GetAIOwner()->GetPawn());

	if (Target && AICharacter)
	{
		float Distance = FVector::Dist(AICharacter->GetActorLocation(), Target->GetActorLocation());
		return Distance <= 200.0f;	// 공격 사정거리
	}

	return false;
}
