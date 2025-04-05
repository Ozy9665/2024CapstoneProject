// Fill out your copyright notice in the Description page of Project Settings.


#include "PoliceAICharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "BehaviorTree/BehaviorTree.h"

APoliceAICharacter::APoliceAICharacter(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

	PrimaryActorTick.bCanEverTick = true;

	// 시야 감지
	AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	SightConfig->SightRadius = 1500.0f;
	SightConfig->LoseSightRadius = 1600.0f;
	SightConfig->PeripheralVisionAngleDegrees = 90.0f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	AIPerceptionComponent->ConfigureSense(*SightConfig);
	AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());

	// Perception 바인딩
	AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &APoliceAICharacter::OnTargetPerceived);

}

void APoliceAICharacter::BeginPlay()
{
	Super::BeginPlay();
}

void APoliceAICharacter::ChaseTarget(AActor* Target)
{
	UE_LOG(LogTemp, Warning, TEXT("Chasing Target: %s"), *Target->GetName());
}

void APoliceAICharacter::AttackTarget()
{
	UE_LOG(LogTemp, Warning, TEXT("Attacking Target"));
}

void APoliceAICharacter::OnTargetPerceived(AActor* Actor, FAIStimulus Stimulus)
{
	// 시야 감지
	if (Stimulus.WasSuccessfullySensed())
	{
		UE_LOG(LogTemp, Warning, TEXT("Target Spotted: %s"), *Actor->GetName());

		// 블랙보드에 타겟 설정하기
		AAIController* AIController = Cast<AAIController>(GetController());
		if (AIController && AIController->GetBlackboardComponent())
		{
			AIController->GetBlackboardComponent()->SetValueAsObject("TargetActor", Actor);
		}
	}
	// 시야 벗어남
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Lost Sight of : %s"), *Actor->GetName());
	}
}