// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "Navigation/PathFollowingComponent.h"

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
	void OnTargetPerceived(AActor* Actor, FAIStimulus Stimulus);
	UFUNCTION()
	void OnTargetDetected(AActor* Actor, FAIStimulus Stimulus);
	FVector GetRandomPatrolLocation();


	// 맵에 배치할 패트롤포인트
	UPROPERTY(EditAnywhere, Category="AI")
	TArray<AActor*> PatrolPoints;


	int32 CurrentPatrolIndex = 0;


	AActor* GetCurrentPatrolPoint();
	void AdvancePatrolPoint();
}; 

