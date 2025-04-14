// Fill out your copyright notice in the Description page of Project Settings.


#include "PerceptionActor.h"

// Sets default values
APerceptionActor::APerceptionActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	SightConfig->SightRadius = 1500.f;
	SightConfig->LoseSightRadius = 1600.0f;
	SightConfig->PeripheralVisionAngleDegrees = 90.0f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;


	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->SetDominantSense(*SightConfig->GetSenseImplementation());

}

// Called when the game starts or when spawned
void APerceptionActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void APerceptionActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

