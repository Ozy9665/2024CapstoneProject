// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowActor.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/SphereComponent.h"
#include "TimerManager.h"
#include "Components/WidgetComponent.h"

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

		// ��Ʈ���߿� ���� ���� ����
		SenseTimer += DeltaTime;
		if (SenseTimer > SenseInterval)
		{
			SenseTimer = 0.f;
			DetectPolice();
		}
	}
	else if (CurrentState == ECrowState::Alert && TargetPolice)
	{
		// ȸ�� ����
		CurrentAngle += FMath::DegreesToRadians(AlertOrbitSpeed) * DeltaTime;

		FVector Center = TargetPolice->GetActorLocation() + FVector(0, 0, 200.f);
		FVector Offset(FMath::Cos(CurrentAngle) * AlertRadius,
			FMath::Sin(CurrentAngle) * AlertRadius, 100.f);

		FVector TargetPos = Center + Offset;
		SetActorLocation(TargetPos);

		FVector ForwardDir = (TargetPos - Center).GetSafeNormal();
		SetActorRotation(ForwardDir.Rotation());

		// �Ÿ�����
		float Dist = FVector::Dist(GetActorLocation(), TargetPolice->GetActorLocation());
		if (Dist > SenseRadius)
		{
			EndAlertState();
		}
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

		if (SpawnElapsed > 1.5f)  // ��Ʈ�ѷ� ��ȯ
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

	// LifeTime �� �ڵ� �Ҹ�
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

	// ȿ��
	UE_LOG(LogTemp, Warning, TEXT("Crow Alert Police"));

	// ����

	// Police ��輱 ( �� �� ���� )  - CustomDepth
	if (USkeletalMeshComponent* Mesh = PoliceTarget->FindComponentByClass<USkeletalMeshComponent>())
	{
		Mesh->SetRenderCustomDepth(true);
		Mesh->SetCustomDepthStencilValue(1);
	}
	// ������?
	/*
	if (UWidgetComponent* Icon = PoliceTarget->FindComponentByClass<UWidgetComponent>())
	{
		Icon->SetVisibility(true);
	}
	*/

	// ����
	// UGameplayStatics::PlaySoundAtLocation(this, CrowSound, GetActorLocation());
	// ����Ʈ
	// UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), CrowAlertVFX, PoliceTarget->GetActorLocation());
}

void ACrowActor::EndAlertState()
{
	if (!TargetPolice) return;

	UE_LOG(LogTemp, Warning, TEXT("End Alert"));

	// ��輱 off
	if (USkeletalMeshComponent* Mesh = TargetPolice->FindComponentByClass<USkeletalMeshComponent>())
	{
		Mesh->SetRenderCustomDepth(false);
	}

	//

	TargetPolice = nullptr;
	CurrentState = ECrowState::Patrol;
	CurrentAngle = 0.f;
}

void ACrowActor::DetectPolice()
{
	// 
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(SenseRadius);

	// ������ �˻�
	bool bHit = GetWorld()->OverlapMultiByChannel(
		Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, Sphere
	);

	if (!bHit) return;

	for (auto& O : Overlaps)
	{
		AActor* Other = O.GetActor();
		if (!Other) continue;

		if (Other->ActorHasTag(TEXT("Police")))
		{
			EnterAlertState(Other);
			break;	// ���� - �ٷ� break
		}
	}
}

void ACrowActor::DestroyCrow()
{
	Destroy();
}