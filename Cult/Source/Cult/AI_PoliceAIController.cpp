// Fill out your copyright notice in the Description page of Project Settings.


#include "AI_PoliceAIController.h"
#include "PoliceAICharacter.h"


void AAI_PoliceAIController::BeginPlay()
{
	Super::BeginPlay();

	/*if (AIBehaviorTree)
	{
		RunBehaviorTree(AIBehaviorTree);
	}*/


}

void AAI_PoliceAIController::StartChase(AActor* Target)
{
	if (Blackboard)
	{
		Blackboard->SetValueAsObject(TEXT("Target"), Target);
	}
}

void AAI_PoliceAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	
	if (UseBlackboard(BlackboardAsset, BlackboardComponent))
	{
		RunBehaviorTree(AIBehaviorTree);
	}
	APoliceAICharacter* AICharacter = Cast<APoliceAICharacter>(InPawn);
	if (AICharacter && AICharacter->PatrolPoints.Num() > 0)
	{
		PatrolPoints = AICharacter->PatrolPoints;
	}

 }

void AAI_PoliceAIController::OnTargetPerceived(AActor* Actor, FAIStimulus Stimulus)
{
	if (Actor->ActorHasTag("Cultist") && Stimulus.WasSuccessfullySensed())
	{
		UE_LOG(LogTemp, Warning, TEXT("Find Cultist! Ganna Chase Target"));
		Blackboard->SetValueAsObject("TargetActor", Actor);
	}
}

FVector AAI_PoliceAIController::GetRandomPatrolLocation()
{
	if (PatrolPoints.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No PatrolPoint"));
		return GetPawn()->GetActorLocation();
	}
	int32 Index = FMath::RandRange(0, PatrolPoints.Num() - 1);
	return PatrolPoints[Index]->GetActorLocation();
}