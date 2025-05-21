// Fill out your copyright notice in the Description page of Project Settings.


#include "TreeObstacleActor.h"

// Sets default values
ATreeObstacleActor::ATreeObstacleActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	TreeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TreeMesh"));
	RootComponent = TreeMesh;

	TreeMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TreeMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TreeMesh->SetSimulatePhysics(false);
}

// Called when the game starts or when spawned
void ATreeObstacleActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATreeObstacleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

