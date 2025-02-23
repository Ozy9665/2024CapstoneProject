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
	
	// 이전 도망 위치
	static FVector LastRunLocation = FVector::ZeroVector;	

	FVector RunDirection = (CultistLocation - PoliceLocation).GetSafeNormal();	// GetSafe(Opposite)
	FVector RunLocation = CultistLocation + (RunDirection * 500.0f);			// 500 back


	// 네비게이션 시스템 가져오기
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	
	if (FVector::Dist(LastRunLocation, RunLocation) > 300.0f)
	{
		LastRunLocation = RunLocation;
		if (NavSys)
		{
			FNavLocation NavLocation;
			// 막히지 않는 RunLocation 근처 300.0f 반경 위치 찾기
			if (NavSys->GetRandomPointInNavigableRadius(RunLocation, 300.0f, NavLocation))
			{
				AIController->MoveToLocation(NavLocation.Location);
				return EBTNodeResult::Succeeded;
			}
		}

	}
	
	return EBTNodeResult::Succeeded;
}