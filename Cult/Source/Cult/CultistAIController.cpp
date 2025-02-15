// Fill out your copyright notice in the Description page of Project Settings.


#include "CultistAIController.h"
#include"Perception/AISense_Sight.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "PoliceCharacter.h"


ACultistAIController::ACultistAIController()
{
	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComp"));
}

void ACultistAIController::BeginPlay()
{
	Super::BeginPlay();

	//
	if (BehaviorTree)
	{
		RunBehaviorTree(BehaviorTree);
	}

	if (AIPerceptionComp)
	{
		AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &ACultistAIController::OnTargetDetected);
	}
}

void ACultistAIController::OnTargetDetected(AActor* Actor, FAIStimulus Stimulus)
{
	if (APoliceCharacter* Police = Cast<APoliceCharacter>(Actor))
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			Blackboard->SetValueAsObject(TEXT("TargetActor"), Police);
			UE_LOG(LogTemp, Warning, TEXT("Police detected!"));
		}
		else {
			Blackboard->ClearValue(TEXT("TargetActor"));
			UE_LOG(LogTemp, Warning, TEXT("Police lost!"));
		}
	}
}
