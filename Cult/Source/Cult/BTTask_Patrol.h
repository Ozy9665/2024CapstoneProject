// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "AI_PoliceAIController.h"
#include "BTTask_Patrol.generated.h"




UCLASS()
class CULT_API UBTTask_Patrol : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_Patrol();

	UPROPERTY()
	AAI_PoliceAIController* AIController;

	UPROPERTY()
	APawn* AIPawn;

	UPROPERTY()
	FVector PatrolTarget;

	UPROPERTY()
	bool bIsMoving = true;

protected:

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)override;

private:



};
