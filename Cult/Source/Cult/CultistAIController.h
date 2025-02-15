// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "Perception/AIPerceptionComponent.h"
#include "CultistAIController.generated.h"

/**
 * 
 */
UCLASS()
class CULT_API ACultistAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	ACultistAIController();

protected:
	virtual void BeginPlay() override;

	void MoveToTarget(AActor* Target);


private:
	UPROPERTY(EditAnywhere, Category="AI")
	class UBehaviorTree* BehaviorTree;

	UFUNCTION()
	void OnTargetDetected(AActor* Actor, FAIStimulus Stimulus);

	UPROPERTY(VisibleAnywhere, Category="AI")
	class UAIPerceptionComponent* AIPerceptionComp;
};
