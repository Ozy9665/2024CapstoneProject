// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PerceptionActor.h"
#include "PoliceCharacter.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Perception/AISenseConfig_Sight.h"


class UAIPerceptionComponent;
class UAISenseConfig_Sight;

#include "PoliceAICharacter.generated.h"


UCLASS()
class CULT_API APoliceAICharacter : public APoliceCharacter
{
	GENERATED_BODY()
	
public:
	APoliceAICharacter(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason)override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="AI")
	class UAIPerceptionComponent* AIPerceptionComponent;

	UPROPERTY()
	UAISenseConfig_Sight* SightConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	UBehaviorTree* AIBehaviorTree;

	UFUNCTION()
	void OnTargetPerceived(AActor* Actor, FAIStimulus Stimulus);

	UPROPERTY(EditInstanceOnly, Category = "AI")
	TArray<AActor*> PatrolPoints;

	// 시야전용 클래스 지정 
	UPROPERTY(EditDefaultsOnly, Category="AI")
	TSubclassOf<class APerceptionActor> PerceptionActorClass;

	// 인스턴스
	UPROPERTY(Transient)
	APerceptionActor* OnlyPerceptionActor;

	void ChaseTarget(AActor* Target);
	void AttackTarget();
	void TurnRightPerception();
	void TurnLeftPerception();

};
