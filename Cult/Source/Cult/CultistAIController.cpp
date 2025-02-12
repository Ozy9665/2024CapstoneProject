// Fill out your copyright notice in the Description page of Project Settings.


#include "CultistAIController.h"
#include"Perception/AIPerceptionComponent.h"
#include"Perception/AISense_Sight.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
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
		AIPerceptionComp->OnPerceptionUpdated.AddDynamic(this, &ACultistAIController::OnTargetDetected);
	}

	if (!PerceptionComponent)
	{
		PerceptionComponent = NewObject<UAIPerceptionComponent>(this);
		PerceptionComponent->RegisterComponent();
	}

	FActorPerceptionBlueprintInfo Info;
}

void ACultistAIController::OnTargetDetected(const TArray<AActor*>& DetectedActors)
{
	for (AActor* Actor : DetectedActors)
	{
		APoliceCharacter* Police = Cast<APoliceCharacter>(Actor);
		if (Police)
		{
			Blackboard->SetValueAsObject(TEXT("TargetActor"), Police);
			UE_LOG(LogTemp, Warning, TEXT("PD!"));
		}
	}
}
