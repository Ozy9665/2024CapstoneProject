// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PoliceCharacter.h"
#include "PoliceAICharacter.generated.h"

/**
 * 
 */
UCLASS()
class CULT_API APoliceAICharacter : public APoliceCharacter
{
	GENERATED_BODY()
	
public:
	APoliceAICharacter(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="AI")
	class UAIPerceptionComponent* AIPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI")
	class UBehaviorTree* BehaviorTree;

	void ChaseTarget(AActor* Target);
	void AttackTarget();
};
