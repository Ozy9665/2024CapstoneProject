#include "BTTask_RunFromPolice.h"
#include "AIController.h"
#include "CultistCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"





UBTTask_RunFromPolice::UBTTask_RunFromPolice()
{
	NodeName = TEXT("Run From Police");
}

EBTNodeResult::Type UBTTask_RunFromPolice::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)return EBTNodeResult::Failed;

	ACultistCharacter* Cultist = Cast<ACultistCharacter>(AIController->GetPawn());
	if (!Cultist) return EBTNodeResult::Failed;

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard) return EBTNodeResult::Failed;

	AActor* PoliceActor = Cast<AActor>(Blackboard->GetValueAsObject("TargetActor"));
	if (!PoliceActor) return EBTNodeResult::Failed;

	// Run
	FVector PoliceLocation = PoliceActor->GetActorLocation();
	FVector CultistLocation = Cultist->GetActorLocation();						
	FVector RunDirection = (CultistLocation - PoliceLocation).GetSafeNormal();	// GetSafe(Opposite)
	FVector RunLocation = CultistLocation + (RunDirection * 500.0f);			// 500 back

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys)
	{
		FNavLocation NavLocation;
		if (NavSys->GetRandomPointInNavigableRadius(RunLocation, 300.0f, NavLocation))
		{
			AIController->MoveToLocation(NavLocation.Location);
			return EBTNodeResult::Succeeded;
		}
	}

	return EBTNodeResult::Succeeded;
}