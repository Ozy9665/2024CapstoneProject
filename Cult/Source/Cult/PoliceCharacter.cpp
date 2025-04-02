// Fill out your copyright notice in the Description page of Project Settings.


#include "PoliceCharacter.h"
#include "CultistCharacter.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include"Components/BoxComponent.h"
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

APoliceCharacter::APoliceCharacter(const FObjectInitializer& ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bIsAttacking = false;
	//
	WalkSpeed = 650.0f;	// more faster than cultist
	CurrentWeapon = EWeaponType::Baton;


	StimulusComponent = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSourceComponent"));
	StimulusComponent->RegisterForSense(TSubclassOf<UAISense>(UAISense_Sight::StaticClass()));
	StimulusComponent->RegisterWithPerceptionSystem();
	
	// 공격 몽타주 확인
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

	// 무기 초기화
	BatonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BatonMesh"));
	BatonMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));

	PistolMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PistolMesh"));
	PistolMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));

	TaserMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TaserMesh"));
	TaserMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));

	// 공격 콜리전
	AttackCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackCollision"));
	AttackCollision->SetupAttachment(BatonMesh);	// 공격콜리전->무기
	AttackCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 총구
	MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	MuzzleLocation->SetupAttachment(GetMesh(), TEXT("MuzzleSocket"));

	CameraComp = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("CameraComp"));
	CameraComp->SetupAttachment(RootComponent);

	if (CameraComp)
	{
		DefaultFOV = CameraComp->FieldOfView;
	}
}

void APoliceCharacter::BeginPlay()	// 초기화
{
	Super::BeginPlay();
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);

	// 무기
	CurrentWeapon = EWeaponType::Baton;


	bIsAttacking = false;




	// melee 콜리전 설정
	CurrentWeapon = EWeaponType::Baton;

	UpdateWeaponVisibility();	// 기본 : 바톤

	// 
	CameraComp = FindComponentByClass<UCameraComponent>();
	if (CameraComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetDefaultFOV"));
		DefaultFOV = CameraComp->FieldOfView; // 기본 FOV 저장
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("CCNUll"));
		CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
		CameraComp->SetupAttachment(RootComponent);
	}

	// 컨트롤러 체크
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("valid PlayerController"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("NO PlayerController"));
	}

	// 조준점 위젯 생성
	if (CrosshairWidgetClass)
	{
		CrosshairWidget = CreateWidget<UUserWidget>(GetWorld(), CrosshairWidgetClass);
		if (CrosshairWidget)
		{
			CrosshairWidget->AddToViewport();
			CrosshairWidget->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void APoliceCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APoliceCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &APoliceCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APoliceCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);

	PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &APoliceCharacter::StartAttack);
	PlayerInputComponent->BindAction("SwitchWeapon", IE_Pressed, this, &APoliceCharacter::SwitchWeapon);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &APoliceCharacter::ToggleCrouch);
	UE_LOG(LogTemp, Warning, TEXT("Binding Aim input"));
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &APoliceCharacter::OnAimPressed);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &APoliceCharacter::StopAiming);
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
	if (bIsAttacking || bIsCoolTime)return;

	bIsAttacking = true; 

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimaInstance NULL"));
	}
	// 애니메이션 길이 - 끝나고 EndAttack 부를 수 있도록
	float BatonMontageDuration = AttackMontage->GetPlayLength();
	switch (CurrentWeapon)
	{
	case EWeaponType::Baton:
		if (AttackMontage)
		{
			UE_LOG(LogTemp, Warning, TEXT("Baton Attack"));
			
			bIsCoolTime = true;

			// 이동불가 + 회전 불가
			//GetCharacterMovement()->DisableMovement();
			//GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			GetCharacterMovement()->MaxWalkSpeed = 0.0f;
			GetCharacterMovement()->GroundFriction = 0.1f; // 마찰율줄이기
			GetCharacterMovement()->bOrientRotationToMovement = false;


			// 앞으로 이동
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
			// 이동불가
			GetCharacterMovement()->DisableMovement();

			UE_LOG(LogTemp, Warning, TEXT("Shoot Pistol"));
			ShootPistol();
		}

		//GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::EndAttack, 1.0f, false);
		break;
	case EWeaponType::Taser:
		if (bIsAiming)
		{
			UE_LOG(LogTemp, Warning, TEXT("Shoot Taser"));
			ShootPistol();
		}

		//GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::EndAttack, 1.0f, false);
		break;
	}

}


void APoliceCharacter::ShootPistol()
{
	bIsShooting = true;
	FHitResult HitResult;
	FVector Start = MuzzleLocation->GetComponentLocation();
	FVector ForwardVector = MuzzleLocation->GetForwardVector();
	FVector End = Start + (ForwardVector * 10000.0f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);
	if (bHit)
	{
		UE_LOG(LogTemp, Warning, TEXT("Particle Effect!"));
		ImpactLoc = HitResult.ImpactPoint;
		//SpawnImpactEffect(HitResult.ImpactPoint);

		// 총구에 나이아가라 이펙트
		if (MuzzleImpactParticle)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(), MuzzleImpactParticle,
				MuzzleLocation->GetComponentLocation(),
				MuzzleLocation->GetComponentRotation()
			);
		}

		// Cultist확인하고 TakeDamage호출
		AActor* HitActor = HitResult.GetActor();
		if (HitActor)
		{
			UE_LOG(LogTemp, Warning, TEXT("HitActor: %s"), *HitActor->GetName());

			ACultistCharacter* Cultist = Cast<ACultistCharacter>(HitActor);

			if (Cultist)
			{
				UE_LOG(LogTemp, Warning, TEXT("Shot Cultist"));
				Cultist->TakeDamage(AttackDamage);
			}
		}

	}
	GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::EndPistolShoot, 1.1f, false);

}



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
	// 공격 범위
	FVector End = Start + (ForwardVector * 200.0f);

	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);

	// 공격범위 충돌체크
	bool bBatonHit = GetWorld()->SweepSingleByChannel(
		HitResult, Start, End, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(50.0f),
		CollisionParams
	);

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
				OnAttackHit(Cultist);
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
		// 데미지
		if (Cultist)
		{
			UE_LOG(LogTemp, Warning, TEXT("OnAttackHit: %s"), *Cultist->GetName());

			Cultist->TakeDamage(AttackDamage);
		}
	}

}

void APoliceCharacter::EndAttack()
{

	// 이동 가능
	//GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->MaxWalkSpeed = 650.0f;
	GetCharacterMovement()->GroundFriction = 8.0f; // 마찰율 복구
	if (!bIsAiming)
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
	}
	bIsAttacking = false;
	UE_LOG(LogTemp, Warning, TEXT("EndAttack"));
	GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::SetCoolTimeDone, fCoolTime, false);

}

void APoliceCharacter::EndPistolShoot()
{
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	bIsShooting = false;
	bIsAttacking = false;

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

	//조준점 활성화
	if (CrosshairWidget)
	{
		CrosshairWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void APoliceCharacter::StopAiming()
{
	bIsAiming = false;
	UE_LOG(LogTemp, Warning, TEXT("StopAiming, bIsAiming is %s"), (bIsAiming ? TEXT("true") : TEXT("false")));

	if (CameraComp)
	{
		CameraComp->SetFieldOfView(DefaultFOV);
	}
	if (!GetCharacterMovement()->bOrientRotationToMovement)
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
	}

	// 조준점 다시 숨김
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
	AddControllerYawInput(Value);
}
void APoliceCharacter::LookUpCamera(float Value)
{
	AddControllerPitchInput(Value);
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