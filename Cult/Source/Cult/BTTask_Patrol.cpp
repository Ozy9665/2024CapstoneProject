// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_Patrol.h"
#include "PoliceCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "AI_PoliceAIController.h"
#include "PoliceAICharacter.h"
#include "AIController.h"


UBTTask_Patrol::UBTTask_Patrol()
{
	NodeName = TEXT("Patrol");
	bNotifyTick = false;
	bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_Patrol::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAI_PoliceAIController* AIController = Cast<AAI_PoliceAIController>(OwnerComp.GetAIOwner());
	if (!AIController) return EBTNodeResult::Failed;

	CachedOwnerComp = &OwnerComp;

	// 랜덤위치 패트롤
	FVector PatrolLocation = AIController->GetRandomPatrolLocation();

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(PatrolLocation);
	MoveRequest.SetAcceptanceRadius(50.0f);

	FNavPathSharedPtr NavPath;
	FPathFollowingRequestResult Result = AIController->MoveTo(MoveRequest, &NavPath);

	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		//AIController->GetPathFollowingComponent()->OnRequestFinished.AddUObject(this, &UBTTask_Patrol::OnMoveCompleted);
		return EBTNodeResult::InProgress;
	}

	return EBTNodeResult::Failed;
}

//void UBTTask_Patrol::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
//{
//	if (CachedOwnerComp)
//	{
//		FinishLatentTask(*CachedOwnerComp, EBTNodeResult::Succeeded);
//	}
//}

void UBTTask_Patrol::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	AAI_PoliceAIController* AIController = Cast<AAI_PoliceAIController>(OwnerComp.GetAIOwner());
	if (AIController)
	{
		AIController->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
	}
}

void UBTTask_Patrol::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{

//	FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}