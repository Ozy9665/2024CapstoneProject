// Fill out your copyright notice in the Description page of Project Settings.


#include "BuildingBlockActor.h"
#include "Components/StaticMeshComponent.h"

// Sets default values
ABuildingBlockActor::ABuildingBlockActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	BlockComponent = CreateDefaultSubobject<UBuildingBlockComponent>(TEXT("BlockComponent"));
	RootComponent = BlockComponent;
}

// Called when the game starts or when spawned
void ABuildingBlockActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABuildingBlockActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABuildingBlockActor::BreakSelf()
{
		// Break
}
