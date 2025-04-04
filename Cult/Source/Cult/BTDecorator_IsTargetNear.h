// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_IsTargetNear.generated.h"

/**
 * 
 */
UCLASS()
class CULT_API UBTDecorator_IsTargetNear : public UBTDecorator
{
	GENERATED_BODY()
	
public:
	UBTDecorator_IsTargetNear();

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};
