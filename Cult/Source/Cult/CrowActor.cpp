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
	else if (CurrentState == ECrowState::Dive && TargetPolice)
	{
		const FVector To = (TargetPolice->GetActorLocation() - GetActorLocation());
		const float Dist = To.Size();
		
		const FVector Dir = To.GetSafeNormal();
		SetActorLocation(GetActorLocation() + Dir * DiveSpeed * DeltaTime);
		SetActorRotation(Dir.Rotation());

		if (Dist < DiveHitRadius)
		{
			ExplodeOnPolice();
			return;
		}
	}

}

// Called to bind functionality to input
void ACrowActor::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("CrowForward", this, &ACrowActor::Axis_CrowForward);
	PlayerInputComponent->BindAxis("CrowRight", this, &ACrowActor::Axis_CrowRight);
	PlayerInputComponent->BindAxis("CrowUp", this, &ACrowActor::Axis_CrowUp);
	PlayerInputComponent->BindAxis("Turn", this, &ACrowActor::Axis_Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &ACrowActor::Axis_LookUp);
}


void ACrowActor::InitCrow(AActor* InOwner, float InLifeTime)
{
	CrowOwner = InOwner;
	OwnerPawn = Cast<APawn>(InOwner);
	PatrolCenter = InOwner ? InOwner->GetActorLocation() : GetActorLocation();
	
	CurrentState = ECrowState::SpawnRise;
	CurrentAngle = 0.f;
	SpawnElapsed = 0.f;

	// PatrolTime �� �ڵ� �Ҹ�
	GetWorldTimerManager().SetTimer(
		PatrolTimer, this, &ACrowActor::OnPatrolExpire, PatrolDuration, false
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

	// Alert�ð� �� �ı�
	GetWorldTimerManager().ClearTimer(AlertTimer);
	GetWorldTimerManager().SetTimer(AlertTimer, this, &ACrowActor::OnAlertExpire, AlertDuration, false);

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


void ACrowActor::RequestDive()
{
	if (CurrentState != ECrowState::Alert || !TargetPolice)return;

	CurrentState = ECrowState::Dive;
	GetWorldTimerManager().ClearTimer(AlertTimer);
}

void ACrowActor::ExplodeOnPolice()
{
	// ����Ʈ / ����
	//

	// ���� UI/���� ó��
	if (AActor* T = TargetPolice)
	{
		//
		if (auto PC = Cast<APawn>(T))
		{
			//
			// 
		}
	}

	EndAlertState();
	DestroyCrow();

}



// Control

void ACrowActor::BeginControl(APlayerController* PC)
{
	if (!PC) return;
	CachedPC = PC;

	// ī�޶� Crow��
	CachedPC->SetViewTargetWithBlend(this, 0.25f);

	CurrentState = ECrowState::Controlled;
	AxisForward = AxisRight = AxisUp = 0.f;
	AxisTurn = AxisLookUp = 0.f;

	// ���� ����
	GetWorldTimerManager().SetTimer(ControlTimer, this, &ACrowActor::OnControlExpire, ControlDuration, false);

	// �Է�
	AutoReceiveInput = EAutoReceiveInput::Player0;
}

void ACrowActor::EndControl(bool bDestroyCrow)
{
	// ī�޶� ����
	if (CachedPC && OwnerPawn)
	{
		CachedPC->SetViewTargetWithBlend(OwnerPawn, 0.25f);
	}
	GetWorldTimerManager().ClearTimer(ControlTimer);

	// ���� ���� 
	DestroyCrow();
}

// Control - Axis
void ACrowActor::Axis_CrowForward(float V) { AxisForward = V; }
void ACrowActor::Axis_CrowRight(float V) { AxisRight = V; }
void ACrowActor::Axis_CrowUp(float V) { AxisUp = V; }
void ACrowActor::Axis_Turn(float V) { AxisTurn = V; }
void ACrowActor::Axis_LookUp(float V) { AxisLookUp = V; }


void ACrowActor::DestroyCrow()
{
	Destroy();
}



// Expire
void ACrowActor::OnPatrolExpire()
{
	DestroyCrow();
}

void ACrowActor::OnAlertExpire()
{
	EndAlertState();
	DestroyCrow();
}

void ACrowActor::OnControlExpire()
{
	EndControl(false);
}