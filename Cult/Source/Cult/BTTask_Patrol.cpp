// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_Patrol.h"
#include "PoliceCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "AI_PoliceAIController.h"
#include "AIController.h"
#include "PoliceAICharacter.h"
#include "NavigationSystem.h"


UBTTask_Patrol::UBTTask_Patrol()
{
	NodeName = TEXT("Patrol");
	bNotifyTick = true;
	bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_Patrol::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);

	AIController = Cast<AAI_PoliceAIController>(OwnerComp.GetAIOwner());
	if (!AIController) return EBTNodeResult::Failed;

	AActor* CurrentPatrolTarget = AIController->GetCurrentPatrolPoint();
	if (!CurrentPatrolTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("Current Patrol is null"));
		return EBTNodeResult::Failed;
	}
	FVector PatrolLocation = CurrentPatrolTarget->GetActorLocation();

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(PatrolLocation);
	MoveRequest.SetAcceptanceRadius(50.0f);

	FNavPathSharedPtr NavPath;
	FPathFollowingRequestResult Result = AIController->MoveTo(MoveRequest, &NavPath);

	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("Called MoveTo %s"), *PatrolLocation.ToString());
		return EBTNodeResult::InProgress;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed Call MoveTo"));
		return EBTNodeResult::Failed;
	}
}


void UBTTask_Patrol::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	AIController = Cast<AAI_PoliceAIController>(OwnerComp.GetAIOwner());
	if (AIController)
	{
		AIController->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
	}
}

void UBTTask_Patrol::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);
	AIController = Cast<AAI_PoliceAIController>(OwnerComp.GetAIOwner());
	if (!AIController) return;
	AIPawn = AIController->GetPawn();

	if (!AIController || !AIPawn || !bIsMoving)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AActor* CurrentPatrolTarget = AIController->GetCurrentPatrolPoint();
	if (!CurrentPatrolTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("Current Patrol is null"));
		return;
	}
	FVector PatrolLocation = CurrentPatrolTarget->GetActorLocation();
	const float Distance = FVector::Dist(AIPawn->GetActorLocation(), PatrolLocation);

	if (Distance <= 100.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Near To PatPoint. Move To Next"));
		AIController->AdvancePatrolPoint();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}