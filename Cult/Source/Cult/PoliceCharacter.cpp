// Fill out your copyright notice in the Description page of Project Settings.


#include "PoliceCharacter.h"
#include "CultistCharacter.h"
#include "MySocketPoliceActor.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include"Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include"Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include"Perception/AIPerceptionComponent.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Kismet/GameplayStatics.h"
#include "BuildingBlockComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

extern AMySocketPoliceActor* MySocketPoliceActor;

APoliceCharacter::APoliceCharacter(const FObjectInitializer& ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bIsAttacking = false;
	//
	WalkSpeed = 620.0f;	// more faster than cultist
	CurrentWeapon = EWeaponType::Baton;


	StimulusComponent = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSourceComponent"));
	StimulusComponent->RegisterForSense(TSubclassOf<UAISense>(UAISense_Sight::StaticClass()));
	StimulusComponent->RegisterWithPerceptionSystem();
	
	// ���� ��Ÿ�� Ȯ��
	if (!AttackMontage)
	{
		static ConstructorHelpers::FObjectFinder<UAnimMontage> MontageAsset(TEXT("AnimMontage'/Game/Cult_Custom/Characters/Police/Animation/AttackMontage.AttackMontage'"));
		if (MontageAsset.Succeeded())
		{
			AttackMontage = MontageAsset.Object;
			UE_LOG(LogTemp, Warning, TEXT("AttackMontage Load"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("AttackMontage Load Fail"));
		}
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT(" AttackMontage is already"));
	}

	// ���� �ʱ�ȭ
	BatonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BatonMesh"));
	BatonMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));

	PistolMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PistolMesh"));
	PistolMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));

	TaserMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TaserMesh"));
	TaserMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));

	// ���� �ݸ���
	AttackCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackCollision"));
	AttackCollision->SetupAttachment(BatonMesh);	// �����ݸ���->����
	AttackCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// �ѱ�
	MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	MuzzleLocation->SetupAttachment(GetMesh(), TEXT("MuzzleSocket"));

	CameraComp = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("CameraComp"));
	CameraComp->SetupAttachment(RootComponent);

	if (CameraComp)
	{
		DefaultFOV = CameraComp->FieldOfView;
	}
}

void APoliceCharacter::BeginPlay()	// �ʱ�ȭ
{
	Super::BeginPlay();
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);

	// ����
	CurrentWeapon = EWeaponType::Baton;


	bIsAttacking = false;




	// melee �ݸ��� ����
	CurrentWeapon = EWeaponType::Baton;

	UpdateWeaponVisibility();	// �⺻ : ����

	// 
	CameraComp = FindComponentByClass<UCameraComponent>();
	if (CameraComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetDefaultFOV"));
		DefaultFOV = CameraComp->FieldOfView; // �⺻ FOV ����
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("CCNUll"));
		CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
		CameraComp->SetupAttachment(RootComponent);
	}

	// ��Ʈ�ѷ� üũ
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("valid PlayerController"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("NO PlayerController"));
	}

	// ������ ���� ����
	if (CrosshairWidgetClass)
	{
		CrosshairWidget = CreateWidget<UUserWidget>(GetWorld(), CrosshairWidgetClass);
		if (CrosshairWidget)
		{
			CrosshairWidget->AddToViewport();
			CrosshairWidget->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (TestTargetActor)
	{
		TestTargetBlock = Cast<UBuildingBlockComponent>(TestTargetActor->GetComponentByClass(UBuildingBlockComponent::StaticClass()));
		if (TestTargetBlock)
		{
			UE_LOG(LogTemp, Warning, TEXT("Target Block Found: %s"), *TestTargetBlock->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to get BuildingBlockComponent from %s"), *TestTargetActor->GetName());
		}
	}


}

void APoliceCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


	if (bIsAiming)
	{
		float RawPitch = GetControlRotation().Pitch;
		float NormalizedPitch = (RawPitch > 180.0f) ? RawPitch - 360.0f : RawPitch;
		float ClampedPitch = FMath::Clamp(NormalizedPitch, -60.0f, 45.0f);
		AimPitch = -ClampedPitch;
	}
	else
	{
		AimPitch = 0.0f;
	}




}

void APoliceCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &APoliceCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APoliceCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &APoliceCharacter::TurnCamera);
	PlayerInputComponent->BindAxis("LookUp", this, &APoliceCharacter::LookUpCamera);

	PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &APoliceCharacter::StartAttack);
	PlayerInputComponent->BindAction("SwitchWeapon", IE_Pressed, this, &APoliceCharacter::SwitchWeapon);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &APoliceCharacter::ToggleCrouch);
	UE_LOG(LogTemp, Warning, TEXT("Binding Aim input"));
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &APoliceCharacter::OnAimPressed);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &APoliceCharacter::StopAiming);

	PlayerInputComponent->BindAction("TestCollapse", IE_Pressed, this, &APoliceCharacter::TestCollapse);
}

void APoliceCharacter::ToggleCrouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void APoliceCharacter::StartAttack()
{
	if (bIsAttacking || bIsCoolTime || bIsShooting)return;

	bIsAttacking = true; 

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimaInstance NULL"));
	}
	// �ִϸ��̼� ���� - ������ EndAttack �θ� �� �ֵ���
	float BatonMontageDuration = AttackMontage->GetPlayLength();
	switch (CurrentWeapon)
	{
	case EWeaponType::Baton:
		if (AttackMontage)
		{
			UE_LOG(LogTemp, Warning, TEXT("Baton Attack"));
			
			bIsCoolTime = true;

			// �̵��Ұ� + ȸ�� �Ұ�
			//GetCharacterMovement()->DisableMovement();
			//GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			GetCharacterMovement()->MaxWalkSpeed = 0.0f;
			GetCharacterMovement()->GroundFriction = 0.1f; // ���������̱�
			GetCharacterMovement()->bOrientRotationToMovement = false;


			// ������ �̵�
			FVector ForwardImpulse = GetActorForwardVector() * AttackPushForce;
			LaunchCharacter(ForwardImpulse, true, false);
			//FVector ForwardDirection = GetActorForwardVector();
			//AddMovementInput(ForwardDirection, AttackPushForce);

			//GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::BatonAttack, 0.3f, false);
			BatonAttack();
			UE_LOG(LogTemp, Warning, TEXT("SetTimer for CheckBatonAttack"));
			GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::EndAttack, BatonMontageDuration-0.7f, false);
		}
		break;

	case EWeaponType::Pistol:
		if (bIsAiming)
		{
			// �̵��Ұ�
			GetCharacterMovement()->DisableMovement();

			UE_LOG(LogTemp, Warning, TEXT("Shoot Pistol"));
			ShootPistol();
		}

		//GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::EndAttack, 1.0f, false);
		break;
	case EWeaponType::Taser:
		if (bIsAiming)
		{
			GetCharacterMovement()->DisableMovement();

			UE_LOG(LogTemp, Warning, TEXT("Shoot Taser"));
			FireTaser();
		}

		//GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::EndAttack, 1.0f, false);
		break;
	}

}


//					============��������==========
void APoliceCharacter::FireTaser()
{
	if (bIsShooting || !bIsAiming)return;
	bIsShooting = true;

	FVector CameraLocation;
	FRotator CameraRotation;
	GetActorEyesViewPoint(CameraLocation, CameraRotation);
	FVector TraceStart = CameraLocation;
	FVector TraceEnd = TraceStart + (CameraRotation.Vector() * 1000.0f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	bTaserHit = GetWorld()->LineTraceSingleByChannel(ParticleResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	if (bTaserHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("Taser Effect"));

		AActor* HitActor = ParticleResult.GetActor();
		if (HitActor)
		{
			UE_LOG(LogTemp, Warning, TEXT("HitActor: %s"), *HitActor->GetName());	

			ACultistCharacter* Cultist = Cast<ACultistCharacter>(HitActor);

			if (Cultist)
			{
				UE_LOG(LogTemp, Warning, TEXT("Shot Cultist"));
				// Cultist->GotHitTaser(this);
				FHitPacket HitPacket;
				HitPacket.AttackerID = my_ID;
				HitPacket.TargetID = Cultist->GetPlayerID();
				HitPacket.Weapon = EWeaponType::Taser;

				MySocketPoliceActor->SendHitData(HitPacket);
			}
		}
	}
}

void APoliceCharacter::EndFireTaser()
{
	// �Ŀ� EndPistolShoot�� �ٸ� ó��
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	bIsShooting = false;
	bIsAttacking = false;
}

//					============���===========
void APoliceCharacter::ShootPistol()
{
	if (bIsShooting || !bIsAiming)return;
	bIsShooting = true;

	// ���1. Muzzle ����
	FVector Start = MuzzleLocation->GetComponentLocation();
	FVector ForwardVector = MuzzleLocation->GetForwardVector();
	FVector End = Start + (ForwardVector * 10000.0f);	// ��Ÿ�

	// ���2. ī�޶����
	FVector CameraLocation;
	FRotator CameraRotation;
	GetActorEyesViewPoint(CameraLocation, CameraRotation);
	FVector TraceStart = CameraLocation;
	FVector TraceEnd = TraceStart + (CameraRotation.Vector() * 10000.0f);	// ��Ÿ�

	// ����
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	// ����ֱ�	( ����� ���� Start or TraceStart / End or TraceEnd )
	bHit = GetWorld()->LineTraceSingleByChannel(ParticleResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	if (bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("Particle Effect!"));

		// CultistȮ���ϰ� TakeDamageȣ��
		AActor* HitActor = ParticleResult.GetActor();
		if (HitActor)
		{
			UE_LOG(LogTemp, Warning, TEXT("HitActor: %s"), *HitActor->GetName());

			ACultistCharacter* Cultist = Cast<ACultistCharacter>(HitActor);

			if (Cultist)
			{
				UE_LOG(LogTemp, Warning, TEXT("Shot Cultist"));
				// Cultist->TakeDamage(AttackDamage);// �浹ó��
				FHitPacket HitPacket;
				HitPacket.AttackerID = my_ID;
				HitPacket.TargetID = Cultist->GetPlayerID();
				HitPacket.Weapon = EWeaponType::Pistol;

				MySocketPoliceActor->SendHitData(HitPacket);
			}
		}

	}
	//GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::EndPistolShoot, 1.1f, false);

}

void APoliceCharacter::EndPistolShoot()
{
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	bIsShooting = false;
	bIsAttacking = false;

}

//					============��������============
void APoliceCharacter::BatonAttack()
{
	if (!this)
	{
		UE_LOG(LogTemp, Error, TEXT("BatonAttack: this is NULL"));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("BatonAttackStart"));
	FVector Start = GetActorLocation();
	FVector ForwardVector = GetActorForwardVector();
	// ���� ����
	FVector End = Start + (ForwardVector * 200.0f);

	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);

	// ���ݹ��� �浹üũ
	bool bBatonHit = GetWorld()->SweepSingleByChannel(
		HitResult, Start, End, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(50.0f),
		CollisionParams
	);
	DrawDebugSphere(GetWorld(), End, 50.0f, 12, FColor::Blue, false, 1.0f);

	UE_LOG(LogTemp, Warning, TEXT("Sweep Result: %s"), bBatonHit ? TEXT("Hit") : TEXT("Miss"));

	if (bBatonHit)
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor)
		{
			UE_LOG(LogTemp, Warning, TEXT("Hit Actor: %s"), *HitActor->GetName());
			ACultistCharacter* Cultist = Cast<ACultistCharacter>(HitActor);
			if (Cultist)
			{
				// �浹ó��
				//OnAttackHit(Cultist);
				FHitPacket HitPacket;
				HitPacket.AttackerID = my_ID;
				HitPacket.TargetID = Cultist->GetPlayerID();
				HitPacket.Weapon = EWeaponType::Baton;

				if (MySocketPoliceActor)
				{
					MySocketPoliceActor->SendHitData(HitPacket);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("MySocketPoliceActor is nullptr!"));
				}

			}
		}
	}
}

void APoliceCharacter::OnAttackHit(AActor* HitActor)
{
	if (HitActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("OnAttackHit Called"));
		ACultistCharacter* Cultist = Cast<ACultistCharacter>(HitActor);
		// ������
		if (Cultist)
		{
			UE_LOG(LogTemp, Warning, TEXT("OnAttackHit: %s"), *Cultist->GetName());

			if (bIsAttacking)
			{

				// �˹�
				FVector Direction = (Cultist->GetActorLocation() - GetActorLocation()).GetSafeNormal();
				float Distance = FVector::Dist(GetActorLocation(), Cultist->GetActorLocation());
				// �Ÿ��� �ݺ��
				float MaxDistance = 200.0f;
				float MinPush = 300.0f;	// �ִ� �Ÿ� ��Ʈ �� �˹�ӷ�
				float MaxPush = 800.0f;

				float Alpha = 1.0f - FMath::Clamp(Distance / MaxDistance, 0.0f, 1.0f);
				float FinalPushStrength = FMath::Lerp(MinPush, MaxPush, Alpha);

				FVector LaunchVelocity = Direction * FinalPushStrength + FVector(0.0f, 0.0f, 100.0f);

				// ��ֹ� ������ �Ÿ�����
				FVector Start = Cultist->GetActorLocation();
				FVector End = Start + Direction * FinalPushStrength;
				// ��ֹ� �����Ǻ� ����Ʈ���̽�
				FHitResult Hit;
				FCollisionQueryParams Params;
				Params.AddIgnoredActor(this);
				Params.AddIgnoredActor(Cultist);
				bool SomethingBehind = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
				if (SomethingBehind)
				{
					float WallDistance = FVector::Dist(Start, Hit.ImpactPoint);
					LaunchVelocity = Direction * WallDistance * 0.9f + FVector(0.f, 0.f, 100.f);
				}
				// ���� �˹�
				Cultist->LaunchCharacter(LaunchVelocity, true, true);

				// ������ ó��
				Cultist->TakeDamage(AttackDamage);
			}
		}
	}

}

void APoliceCharacter::EndAttack()
{

	// �̵� ����
	//GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->MaxWalkSpeed = 620.0f;
	GetCharacterMovement()->GroundFriction = 8.0f; // ������ ����
	if (!bIsAiming)
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
	}
	bIsAttacking = false;
	UE_LOG(LogTemp, Warning, TEXT("EndAttack"));
	GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::SetCoolTimeDone, fCoolTime, false);

}



void APoliceCharacter::SetCoolTimeDone()
{
	bIsCoolTime = false;
}





void APoliceCharacter::SwitchWeapon()
{
	if (bIsAiming)return;
	if (bIsShooting)return;
	if (bIsAttacking)return;
	if (CurrentWeapon == EWeaponType::Baton)
		CurrentWeapon = EWeaponType::Pistol;
	else if (CurrentWeapon == EWeaponType::Pistol)
		CurrentWeapon = EWeaponType::Taser;
	else
		CurrentWeapon = EWeaponType::Baton;

	UpdateWeaponVisibility();
}

void APoliceCharacter::UpdateWeaponVisibility()
{
	BatonMesh->SetVisibility(CurrentWeapon == EWeaponType::Baton);
	PistolMesh->SetVisibility(CurrentWeapon == EWeaponType::Pistol);
	TaserMesh->SetVisibility(CurrentWeapon == EWeaponType::Taser);
}

void APoliceCharacter::OnAimPressed()
{
	if (CurrentWeapon == EWeaponType::Baton)return;
	UE_LOG(LogTemp, Warning, TEXT("OnAimPressed"));
	StartAiming();
}

void APoliceCharacter::StartAiming()
{
	UE_LOG(LogTemp, Warning, TEXT("StartAiming"));

	bIsAiming = true;
	UE_LOG(LogTemp, Warning, TEXT("StartAiming, bIsAiming is %s"), (bIsAiming ? TEXT("true"): TEXT("false")));

	GetCharacterMovement()->MaxWalkSpeed = 200.0f;


	//������ Ȱ��ȭ
	if (CrosshairWidget)
	{
		CrosshairWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void APoliceCharacter::StopAiming()
{
	bIsAiming = false;
	UE_LOG(LogTemp, Warning, TEXT("StopAiming, bIsAiming is %s"), (bIsAiming ? TEXT("true") : TEXT("false")));

	GetCharacterMovement()->MaxWalkSpeed = 620.0f;

	if (CameraComp)
	{
		CameraComp->SetFieldOfView(DefaultFOV);
	}
	if (!GetCharacterMovement()->bOrientRotationToMovement)
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
	}

	// ������ �ٽ� ����
	if (CrosshairWidget)
	{
		CrosshairWidget->SetVisibility(ESlateVisibility::Hidden);
	}
}

void APoliceCharacter::SpawnImpactEffect(FVector ImpactLocation)
{
	if (ImpactParticle)
	{
		ImpactParticleComp = UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(), ImpactParticle, ImpactLocation, FRotator::ZeroRotator, true
		);

		//if (ImpactParticleComp)
		//{
		//	ImpactParticleComp->SetAutoDestroy(true);

		//	FTimerHandle TimerHandle;
		//	GetWorldTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateLambda([ImpactParticleComp]()
		//		{
		//			if (ImpactParticleComp)
		//			{
		//				ImpactParticleComp->DestroyComponent();
		//			}
		//		}), 2.0f, false);
		//}
	}
}



// Interaction
void APoliceCharacter::TryPickUp()
{
	TArray<AActor*> Overlapping;

	for (AActor* Actor : Overlapping)
	{
		ACultistCharacter* Cultist = Cast<ACultistCharacter>(Actor);
		// ������ && ���� ���ĸ����� ���� ���¶��

			// ���ĸű�
			// ó�� (Cultist->bIsBeingCarried = true, CarriedCharacter ��
			// break;
	}
}

void APoliceCharacter::TryConfine(AActor* ConfineTarget)
{
	if (CarriedCharacter) // && ������Ʈ�� ���� ��
	{
		

		// ó��
		CarriedCharacter->bIsConfined = true;
		CarriedCharacter->bIsBeingCarried = false;
		CarriedCharacter = nullptr;
	}
}



// Movement
void APoliceCharacter::MoveForward(float Value)
{
	if (Controller && Value != 0.0f)
	{
		FRotator ControlRotation = GetControlRotation();
		FRotator YawRotation(0, ControlRotation.Yaw, 0);

		FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}
void APoliceCharacter::MoveRight(float Value)
{
	if (Controller && Value != 0.0f)
	{
		FRotator ControlRotation = GetControlRotation();
		FRotator YawRotation(0, ControlRotation.Yaw, 9);

		FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
}


void APoliceCharacter::TurnCamera(float Value)
{
	float TurnCameraSpeed = bIsAiming ? 0.2f : 0.4f;
	AddControllerYawInput(Value * TurnCameraSpeed);
}
void APoliceCharacter::LookUpCamera(float Value)
{
	float LookUpSpeed = bIsAiming ? 0.2f : 0.4f;
	AddControllerPitchInput(Value * LookUpSpeed);
}
void APoliceCharacter::TurnCharacter()
{
	FVector MovementDirection = GetVelocity().GetSafeNormal();

	if (!MovementDirection.IsZero())
	{
		FRotator NewRotation = FRotationMatrix::MakeFromX(MovementDirection).Rotator();
		FRotator CurrentRotation = GetActorRotation();
		FRotator DesiredRotation = FRotator(0.0f, NewRotation.Yaw, 0.0f);
		FRotator SmoothRotation = FMath::RInterpTo(CurrentRotation, DesiredRotation, GetWorld()->GetDeltaSeconds(), 5.0f);

		SetActorRotation(SmoothRotation);
	}
}

void APoliceCharacter::VaultStart()
{
	IsPakour = true;
}
void APoliceCharacter::VaultEnd()
{
	IsPakour = false;
}

void APoliceCharacter::TestCollapse()
{
	if (TestTargetBlock)
	{
		FVector ImpulseDirection = FVector::UpVector;

		UE_LOG(LogTemp, Warning, TEXT("Sending impulse to block"));
		TestTargetBlock->ReceiveImpulse(ImpulseAmount, ImpulseDirection);

		DrawDebugSphere(GetWorld(), TestTargetBlock->GetComponentLocation(), 30.f, 12, FColor::Red, false, 2.0f);
	}
}