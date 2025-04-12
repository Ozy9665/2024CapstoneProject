// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

#include "BTTask_Patrol.generated.h"

/**
 * 
 */
UCLASS()
class CULT_API UBTTask_Patrol : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_Patrol();

protected:

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)override;
	UFUNCTION()
	void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);

private:


	FDelegateHandle MoveCompletedHandle;
	UBehaviorTreeComponent* CachedOwnerComp;
};
