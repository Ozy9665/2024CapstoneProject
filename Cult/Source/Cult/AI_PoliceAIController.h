// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI_PoliceAIController.generated.h"

/**
 * 
 */
UCLASS()
class CULT_API AAI_PoliceAIController : public AAIController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditDefaultsOnly, Category="AI")
	class UBehaviorTree* AIBehaviorTree;

	void StartChase(AActor* Target);
};
