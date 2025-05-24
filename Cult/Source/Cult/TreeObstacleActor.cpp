// Fill out your copyright notice in the Description page of Project Settings.


#include "TreeObstacleActor.h"

// Sets default values
ATreeObstacleActor::ATreeObstacleActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	TreeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TreeMesh"));
	RootComponent = TreeMesh;

	TreeMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TreeMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TreeMesh->SetSimulatePhysics(false);

	// Spline √ ±‚»≠
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	Spline->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void ATreeObstacleActor::BeginPlay()
{
	Super::BeginPlay();
	
	InitialLocation = GetActorLocation();
	TargetLocation = InitialLocation + FVector(0.f, 0.f, GrowHeight);

	SetActorLocation(InitialLocation - FVector(0.f, 0.f, GrowHeight));
}

// Called every frame
void ATreeObstacleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsGrowing)
	{
		ElapsedTime += DeltaTime;
		float Alpha = FMath::Clamp(ElapsedTime / GrowTime, 0.f, 1.f);
		FVector NewLocation = FMath::Lerp(InitialLocation - FVector(0, 0, GrowHeight), InitialLocation, Alpha);
		SetActorLocation(NewLocation);

		if (Alpha >= 1.f)
		{
			bIsGrowing = false;
		}
	}
}

