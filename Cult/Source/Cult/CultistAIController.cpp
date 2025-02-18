// Fill out your copyright notice in the Description page of Project Settings.


#include "CultistAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include"Perception/AISense_Sight.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Altar.h"
#include "Kismet/GameplayStatics.h"
#include "PoliceCharacter.h"


ACultistAIController::ACultistAIController()
{
	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComp"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1000.0f;
	SightConfig->LoseSightRadius = 1200.0f;
	SightConfig->PeripheralVisionAngleDegrees = 90.0f;
	SightConfig->SetMaxAge(5.0f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(*SightConfig->GetSenseImplementation());
	//AIPerceptionComp->OnPerceptionUpdated.AddDynamic(this, &ACultistAIController::OnTargetDetected);
	AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &ACultistAIController::OnTargetDetected);

	bIsTargetVisible = false;

	UAIPerceptionComponent* PerceptionComp = FindComponentByClass<UAIPerceptionComponent>();
	if (!PerceptionComp)
	{
		UE_LOG(LogTemp, Error, TEXT("Perception Component None!"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Perception Component Exist!"));
	}
}

void ACultistAIController::BeginPlay()
{
	Super::BeginPlay();
	RunBehaviorTree(BehaviorTree);
	FindAndSetNearestAltar();
}

void ACultistAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

/*
void ACultistAIController::OnTargetDetected(const TArray<AActor*>& DetectedActors)
{
	bool bDetected = false;


	for (AActor* Actor : DetectedActors)
	{
		if (APoliceCharacter* Police = Cast<APoliceCharacter>(Actor))
		{
			Blackboard->SetValueAsObject(TEXT("TargetActor"), Police);
			UE_LOG(LogTemp, Warning, TEXT("Police detected!"));

			GetWorldTimerManager().ClearTimer(ClearTargetHandle);

			bIsTargetVisible = true;
			bDetected = true;
			break;
		}
	}
	if (!bDetected && bIsTargetVisible)
	{
		OnTargetLost();
	}
}
*/

void ACultistAIController::OnTargetDetected(AActor* Actor, FAIStimulus Stimulus)
{
	APoliceCharacter* Police = Cast<APoliceCharacter>(Actor);
	if (!Police) return;

	if (Stimulus.WasSuccessfullySensed()) // °æÂûÀÌ °¨ÁöµÊ
	{
		Blackboard->SetValueAsObject(TEXT("TargetActor"), Police);
		UE_LOG(LogTemp, Warning, TEXT("Police detected!"));

		// ±âÁ¸ Å¸ÀÌ¸Ó Á¦°Å
		GetWorldTimerManager().ClearTimer(ClearTargetHandle);
	}
	else // °æÂûÀÌ »ç¶óÁü
	{
		UE_LOG(LogTemp, Warning, TEXT("Police lost! Clearing target in 3 seconds..."));
		GetWorldTimerManager().SetTimer(ClearTargetHandle, this, &ACultistAIController::ClearTargetActor, 3.0f, false);
	}
}
/*
void ACultistAIController::OnTargetLost()
{
	UE_LOG(LogTemp, Warning, TEXT("Police Lost, Target will be cleared in 3 seconds.."));

	bIsTargetVisible = false;

	GetWorldTimerManager().SetTimer(ClearTargetHandle, this, &ACultistAIController::ClearTargetActor, 3.0f, false);
}
*/

void ACultistAIController::ClearTargetActor()
{
	Blackboard->ClearValue(TEXT("TargetActor"));
	UE_LOG(LogTemp, Warning, TEXT("Police lost!"));
}

void ACultistAIController::FindAndSetNearestAltar()
{
	TArray<AActor*> FoundAltars;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAltar::StaticClass(), FoundAltars);

	if (FoundAltars.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("No Altar found in the level"));
		return;
	}

	AActor* NearestAltar = nullptr;
	float NearestDistance = FLT_MAX;
	FVector AI_Location = GetPawn()->GetActorLocation();


	for (AActor* Altar : FoundAltars)
	{
		float Distance = FVector::Dist(AI_Location, Altar->GetActorLocation());

		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			NearestAltar = Altar;
		}
	}

	if (NearestAltar)
	{
		FVector AltarLocation = NearestAltar->GetActorLocation();
		Blackboard->SetValueAsVector(TEXT("AltarLocation"), AltarLocation);

		UE_LOG(LogTemp, Warning, TEXT("Nearest AltarLocation Set: X=%f, Y=%f, Z=%f"),
			AltarLocation.X, AltarLocation.Y, AltarLocation.Z);
	}
}