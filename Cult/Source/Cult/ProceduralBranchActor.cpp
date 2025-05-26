// Fill out your copyright notice in the Description page of Project Settings.


#include "ProceduralBranchActor.h"

// Sets default values
AProceduralBranchActor::AProceduralBranchActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AProceduralBranchActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AProceduralBranchActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AProceduralBranchActor::GenerateCurvedBranch(float Length, float RadiusStart, float RadiusEnd, int32 Segments)
{
	UE_LOG(LogTemp, Warning, TEXT("GenerateCurvedBranch »£√‚µ . Length = %.2f"), Length);
}
void AProceduralBranchActor::GenerateCylinderAlongSpline(const TArray<FVector>& SplinePoints, float RadiusStart, float RadiusEnd)
{

}