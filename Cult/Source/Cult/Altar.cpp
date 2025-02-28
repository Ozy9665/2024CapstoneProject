// Fill out your copyright notice in the Description page of Project Settings.


#include "Altar.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "CultistCharacter.h"

TSet<AActor*> PlayersInAltar;

// Sets default values
AAltar::AAltar()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// 제단 메쉬
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;



	//if (!CollisionComp->OnComponentBeginOverlap.IsBound())
	//{
	//	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AAltar::OnOverlapBegin);
	//	UE_LOG(LogTemp, Warning, TEXT("OnOverlapBegin manually bound!"));
	//}

	// 메쉬 설정
	//static ConstructorHelpers::FObjectFinder<UStaticMesh> AltarMesh(TEXT("StaticMesh'/Game/Cult_Custom/Modeling/altar5.altar5'"));
	//if (AltarMesh.Succeeded())
	//{
	//	MeshComp->SetStaticMesh(AltarMesh.Object);
	//}
	NumCultistsInRange = 0;

	bPlayerInRange = false;
}

// Called when the game starts or when spawned
void AAltar::BeginPlay()
{
	Super::BeginPlay();
	
	PlayersInAltar.Empty();


	UStaticMesh* MeshAsset = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("StaticMesh'/Game/Cult_Custom/Modeling/altar5.altar5'")));
	if (MeshAsset)
	{
		MeshComp->SetStaticMesh(MeshAsset);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("StaticLoadObject failed to load StaticMesh!"));
	}

	CollisionComp = Cast<UBoxComponent>(GetDefaultSubobjectByName(TEXT("CollisionComp2")));
	if (CollisionComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("CollisionComp On"));

		CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AAltar::OnOverlapBegin);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Check Collision!"));
	}

	// 충돌 이벤트 바인드
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AAltar::OnOverlapBegin);
	CollisionComp->OnComponentEndOverlap.AddDynamic(this, &AAltar::OnOverlapEnd);

}


void AAltar::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	
	ACultistCharacter* Cultist = Cast<ACultistCharacter>(OtherActor);
	if (Cultist)
	{
		if (!PlayersInAltar.Contains(Cultist))
		{
			PlayersInAltar.Add(Cultist);
			NumCultistsInRange = PlayersInAltar.Num();
		}
		Cultist->SetCurrentAltar(this);
		//NumCultistsInRange++;
		UE_LOG(LogTemp, Warning, TEXT("Cultist entered the altar area"));
	}
	if (OtherActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("Overlap with %s"), *OtherActor->GetName());
	}
}

void AAltar::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{

	// 떠날 때
	ACultistCharacter* Cultist = Cast<ACultistCharacter>(OtherActor);
	if (Cultist)
	{
		PlayersInAltar.Remove(Cultist);
		NumCultistsInRange = PlayersInAltar.Num();
		Cultist->SetCurrentAltar(nullptr);
		//NumCultistsInRange = FMath::Max(0, NumCultistsInRange - 1);	// 최소 0
		UE_LOG(LogTemp, Warning, TEXT("Cultist left the altar area"));
	}
}

// Called every frame
void AAltar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


void AAltar::IncreaseRitualGauge()
{
	if (NumCultistsInRange > 0)
	{
		float GainRate = BaseGainRate * NumCultistsInRange;
		RitualGauge += GainRate;
		RitualGauge = FMath::Clamp(RitualGauge, 0.0f, 100.0f);
		
		UE_LOG(LogTemp, Warning, TEXT("Ritual Progress: %f"), RitualGauge);

		// 게임모드에서 100 넘었는지 체크하고, 넘었으면 레벨 재시작 
		ACultGameMode* GameMode = Cast<ACultGameMode>(GetWorld()->GetAuthGameMode());
		if (GameMode)
		{
			GameMode->CheckRitualComlete(RitualGauge);
		}
	}
}

