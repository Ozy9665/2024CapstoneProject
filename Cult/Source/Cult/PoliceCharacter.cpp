// Fill out your copyright notice in the Description page of Project Settings.


#include "PoliceCharacter.h"
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
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &APoliceCharacter::StartAiming);
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

			GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::BatonAttack, 0.3f, false);
			UE_LOG(LogTemp, Warning, TEXT("SetTimer for CheckBatonAttack"));
			GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::EndAttack, BatonMontageDuration, false);
		}
		break;

	case EWeaponType::Pistol:
		UE_LOG(LogTemp, Warning, TEXT("Shoot Pistol"));
		ShootPistol();
		//GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::EndAttack, 1.0f, false);
		break;
	case EWeaponType::Taser:
		UE_LOG(LogTemp, Warning, TEXT("Shoot Taser"));
		ShootPistol();
		//GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::EndAttack, 1.0f, false);
		break;
	}

}


void APoliceCharacter::ShootPistol()
{
	if (BulletClass)
	{
		// 현재 카메라 방향을 가져옴
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			FVector CameraLocation;
			FRotator CameraRotation;
			PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

			// 총구 위치
			FVector MuzzlePosition = MuzzleLocation->GetComponentLocation();
			FRotator MuzzleRotation = CameraRotation;

			// 총알 생성
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			ABullet* Bullet = GetWorld()->SpawnActor<ABullet>(BulletClass, MuzzlePosition, MuzzleRotation, SpawnParams);
			if (Bullet)
			{
				Bullet->SetDirection(MuzzleRotation.Vector()); // 총알 방향 설정
			}
		}
	}
	EndAttack();
}



void APoliceCharacter::BatonAttack()
{
	UE_LOG(LogTemp, Warning, TEXT("CheckBaton"));
	AttackCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	TArray<AActor*> HitActors;
	AttackCollision->GetOverlappingActors(HitActors);

	for (AActor* Actor : HitActors)
	{
		if (Actor != this)
		{
			UE_LOG(LogTemp, Warning, TEXT("Hit : %s"), *Actor->GetName());
			UGameplayStatics::ApplyDamage(Actor, AttackDamage, GetController(), this, UDamageType::StaticClass());
		}
	}
	AttackCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APoliceCharacter::EndAttack()
{

	// 이동 가능
	//GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->MaxWalkSpeed = 650.0f;
	GetCharacterMovement()->GroundFriction = 8.0f; // 마찰율 복구
	GetCharacterMovement()->bOrientRotationToMovement = true;

	bIsAttacking = false;
	UE_LOG(LogTemp, Warning, TEXT("EndAttack"));
	GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::SetCoolTimeDone, fCoolTime, false);

}

void APoliceCharacter::SetCoolTimeDone()
{
	bIsCoolTime = false;
}

void APoliceCharacter::OnAttackHit()
{
	AttackCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	TArray<AActor*> HitActors;
	AttackCollision->GetOverlappingActors(HitActors);

	for (AActor* Actor : HitActors)
	{
		if (Actor != this)
		{
			// 맞은 캐릭터에게 데미지
			UGameplayStatics::ApplyDamage(Actor, AttackDamage, GetController(), this, UDamageType::StaticClass());
		}
	}

	AttackCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}



void APoliceCharacter::SwitchWeapon()
{
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


void APoliceCharacter::StartAiming()
{
	bIsAiming = true;
	//if (APlayerController* PC = Cast<APlayerController>(GetController()))
 //   {
 //       if (PC->GetViewTarget() == this)
 //       {
 //           UE_LOG(LogTemp, Warning, TEXT("StartAiming, AimFOV is %f"), AimFOV);

 //           // ChildActor에서 실제 카메라 액터 가져오기
 //           AActor* CameraActor = PlayerCameraChild->GetChildActor();
 //           if (CameraActor)
 //           {
 //               // 블루프린트 기반 카메라 액터에서 UCameraComponent 찾기
 //               UCameraComponent* CameraComp = CameraActor->FindComponentByClass<UCameraComponent>();
 //               if (CameraComp)
 //               {
 //                   CameraComp->SetFieldOfView(AimFOV);
 //                   UE_LOG(LogTemp, Warning, TEXT("Aiming..FOV now : %f"), CameraComp->FieldOfView);
 //               }
 //               else
 //               {
 //                   UE_LOG(LogTemp, Error, TEXT("CameraComp not found on BP_PlayerCamera!"));
 //               }
 //           }
 //           else
 //           {
 //               UE_LOG(LogTemp, Error, TEXT("PlayerCameraChild has no valid ChildActor!"));
 //           }
 //       }
 //   }
}

void APoliceCharacter::StopAiming()
{
	bIsAiming = false;
	UE_LOG(LogTemp, Warning, TEXT("StopAiming"));

	if (CameraComp)
	{
		CameraComp->SetFieldOfView(DefaultFOV);
		UE_LOG(LogTemp, Warning, TEXT("SetFOV"));
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
