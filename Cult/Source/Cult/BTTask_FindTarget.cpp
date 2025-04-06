// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_FindTarget.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"

UBTTask_FindTarget::UBTTask_FindTarget()
{
	NodeName = TEXT("Find Target");
}

EBTNodeResult::Type UBTTask_FindTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	APawn* AIPawn = OwnerComp.GetAIOwner()->GetPawn();
	if (!AIPawn)return EBTNodeResult::Failed;

	// Cultist ã��
	TArray<AActor*> PotentialTargets;
	UGameplayStatics::GetAllActorsWithTag(AIPawn->GetWorld(), FName("Cultist"), PotentialTargets);

	if (PotentialTargets.Num() > 0)
	{
		AActor* NearestTarget = PotentialTargets[0];
		float MinDistance = FVector::Dist(AIPawn->GetActorLocation(), NearestTarget->GetActorLocation());
		
		for (AActor* Target : PotentialTargets)
		{
			float Dist = FVector::Dist(AIPawn->GetActorLocation(), Target->GetActorLocation());
			if (Dist < MinDistance)
			{
				MinDistance = Dist;
				NearestTarget = Target;
			}
		}

		// �����忡 ���尡��� ������ Ÿ�꼳��
		OwnerComp.GetBlackboardComponent()->SetValueAsObject("TargetActor", NearestTarget);
		return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::Failed;
}
