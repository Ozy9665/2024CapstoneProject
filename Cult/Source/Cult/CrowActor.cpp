// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowActor.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/SphereComponent.h"

// Sets default values
ACrowActor::ACrowActor()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ACrowActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ACrowActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ACrowActor::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}


void ACrowActor::InitCrow(AActor* InOwner, float InLifeTime)
{

}

void ACrowActor::EnterAlertState(AActor* PoliceTarget)
{

}

void ACrowActor::DestroyCrow()
{
	Destroy();
}