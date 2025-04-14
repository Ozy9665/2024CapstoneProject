// Fill out your copyright notice in the Description page of Project Settings.


#include "AI_PoliceAIController.h"
#include "PoliceAICharacter.h"
#include "GenericTeamAgentInterface.h"


void AAI_PoliceAIController::BeginPlay()
{
	Super::BeginPlay();

	/*if (AIBehaviorTree)
	{
		RunBehaviorTree(AIBehaviorTree);
	}*/

	APoliceAICharacter* AIChar = Cast<APoliceAICharacter>(GetPawn());
	if (AIChar && AIChar->OnlyPerceptionActor)
	{
		AIChar->OnlyPerceptionActor->PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
			this, &AAI_PoliceAIController::OnTargetDetected
		);
		UE_LOG(LogTemp, Warning, TEXT("Bind Detect Function"));
	}
	SetGenericTeamId(FGenericTeamId(1));

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

	UE_LOG(LogTemp, Warning, TEXT("OnlyPerceptionActor is %s"), AICharacter->OnlyPerceptionActor ? TEXT("Valid") : TEXT("NULL"));
	if (AICharacter && AICharacter->OnlyPerceptionActor)
	{
		AICharacter->OnlyPerceptionActor->PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
			this, &AAI_PoliceAIController::OnTargetDetected
		);
		UE_LOG(LogTemp, Warning, TEXT("Bind Detect Function"));
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

	//UE_LOG(LogTemp, Warning, TEXT("Patrol 목표 위치: %s"), *PatrolPoints[Index]->GetActorLocation().ToString());
	return PatrolPoints[Index]->GetActorLocation();
}

AActor* AAI_PoliceAIController::GetCurrentPatrolPoint()
{
	if (PatrolPoints.IsValidIndex(CurrentPatrolIndex))
	{
		//UE_LOG(LogTemp, Warning, TEXT("Patrol 목표 위치: %s"), *PatrolPoints[CurrentPatrolIndex]->GetActorLocation().ToString());

		return PatrolPoints[CurrentPatrolIndex];
	}
	return nullptr;
}

void AAI_PoliceAIController::AdvancePatrolPoint()
{
	CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PatrolPoints.Num();
}

void AAI_PoliceAIController::OnTargetDetected(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor || !Stimulus.WasSuccessfullySensed())return;
	if (Actor->ActorHasTag("Cultist"))
	{
		Blackboard->SetValueAsObject("TargetActor", Actor);
		UE_LOG(LogTemp, Warning, TEXT("Cultist Detected!"));
	}
}