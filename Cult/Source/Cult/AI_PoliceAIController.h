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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
	UBehaviorTree* AIBehaviorTree;

	UPROPERTY(EditDefaultsOnly, Category="AI")
	UBlackboardData* BlackboardAsset;

	UBlackboardComponent* BlackboardComponent;

	void StartChase(AActor* Target);
	virtual void OnPossess(APawn* InPawn) override;
}; 

