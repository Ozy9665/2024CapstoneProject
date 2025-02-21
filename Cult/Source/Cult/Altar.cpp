// Fill out your copyright notice in the Description page of Project Settings.


#include "Altar.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "CultistCharacter.h"

// Sets default values
AAltar::AAltar()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// 제단 메쉬
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	// 콜리전 생성
	CollisionComp = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComp"));
	CollisionComp->SetupAttachment(MeshComp);
	CollisionComp->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));

	// 충돌 이벤트 바인드
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AAltar::OnOverlapBegin);
	CollisionComp->OnComponentEndOverlap.AddDynamic(this, &AAltar::OnOverlapEnd);

	bPlayerInRange = false;
}

// Called when the game starts or when spawned
void AAltar::BeginPlay()
{
	Super::BeginPlay();
	
}

void AAltar::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACultistCharacter* Cultist = Cast<ACultistCharacter>(OtherActor);
	if (Cultist)
	{
		Cultist->SetCurrentAltar(this);
		bPlayerInRange = true;
		UE_LOG(LogTemp, Warning, TEXT("Cultist entered the altar area"));
	}
}

void AAltar::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{

	// 떠날 때
	ACultistCharacter* Cultist = Cast<ACultistCharacter>(OtherActor);
	if (Cultist)
	{
		Cultist->SetCurrentAltar(nullptr);
		bPlayerInRange = false;
		UE_LOG(LogTemp, Warning, TEXT("Cultist left the altar area"));
	}
}

// Called every frame
void AAltar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

