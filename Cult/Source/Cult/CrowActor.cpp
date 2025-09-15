// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowActor.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/SphereComponent.h"
#include "TimerManager.h"

// Sets default values
ACrowActor::ACrowActor()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CrowMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CrowMesh"));
	SetRootComponent(CrowMesh);
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

	if (CurrentState == ECrowState::Patrol)
	{
		CurrentAngle += FMath::DegreesToRadians(OrbitSpeed) * DeltaTime;
		float Radius = 100.f + 50.f * SpawnElapsed ;
		float Height = 100.f + 200.f * SpawnElapsed ;

		FVector Offset(
			FMath::Cos(CurrentAngle) * Radius,
			FMath::Sin(CurrentAngle) * Height,
			PatrolHeight
		);

		FVector TargetPos = PatrolCenter + Offset;
		SetActorLocation(TargetPos);

		FVector ForwardDir = (TargetPos - PatrolCenter).GetSafeNormal();
		SetActorRotation(ForwardDir.Rotation());
	}
	else if (CurrentState == ECrowState::Alert && TargetPolice)
	{
		CurrentAngle += FMath::DegreesToRadians(AlertOrbitSpeed) * DeltaTime;

		FVector Center = TargetPolice->GetActorLocation() + FVector(0, 0, 200.f);
		FVector Offset(FMath::Cos(CurrentAngle) * AlertRadius,
			FMath::Sin(CurrentAngle) * AlertRadius, 100.f);

		FVector TargetPos = Center + Offset;
		SetActorLocation(TargetPos);

		FVector ForwardDir = (TargetPos - Center).GetSafeNormal();
		SetActorRotation(ForwardDir.Rotation());
	}
	else if (CurrentState == ECrowState::SpawnRise)
	{
		SpawnElapsed += DeltaTime;


		CurrentAngle += FMath::DegreesToRadians(OrbitSpeed) * DeltaTime;
		float Radius = 100.f + 100.f * SpawnElapsed;   
		float Height = 50.f + 300.f * SpawnElapsed;    

		FVector Offset(
			FMath::Cos(CurrentAngle) * Radius,
			FMath::Sin(CurrentAngle) * Radius,
			Height
		);
		SetActorLocation(PatrolCenter + Offset);

		if (SpawnElapsed > 1.5f)  // 패트롤로 전환
		{
			CurrentState = ECrowState::Patrol;
			CurrentAngle = 0.f;
		}
	}

}

// Called to bind functionality to input
void ACrowActor::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}


void ACrowActor::InitCrow(AActor* InOwner, float InLifeTime)
{
	CrowOwner = InOwner;
	PatrolCenter = InOwner ? InOwner->GetActorLocation() : GetActorLocation();
	CurrentState = ECrowState::SpawnRise;

	CurrentAngle = 0.f;
	SpawnElapsed = 0.f;

	// LifeTime 후 자동 소멸
	GetWorldTimerManager().SetTimer(
		LifeTimer, this, &ACrowActor::DestroyCrow, InLifeTime, false
	);
}

void ACrowActor::EnterAlertState(AActor* PoliceTarget)
{
	if (!PoliceTarget) return;

	CurrentState = ECrowState::Alert;
	TargetPolice = PoliceTarget;
	CurrentAngle = 0.f;
}

void ACrowActor::DestroyCrow()
{
	Destroy();
}