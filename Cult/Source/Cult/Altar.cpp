// Fill out your copyright notice in the Description page of Project Settings.


#include "Altar.h"
#include "CultistCharacter.h"

// Sets default values
AAltar::AAltar()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = MeshComponent;

	// 
	OnActorBeginOverlap.AddDynamic(this, &AAltar::OnBeginOverlap);
}

// Called when the game starts or when spawned
void AAltar::BeginPlay()
{
	Super::BeginPlay();
	
}

void AAltar::OnBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	ACultistCharacter* Cultist = Cast<ACultistCharacter>(OtherActor);
	if (Cultist)
	{
		Cultist->CurrentAltar = this;
	}
}

// Called every frame
void AAltar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

