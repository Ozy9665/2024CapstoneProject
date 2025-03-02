// Fill out your copyright notice in the Description page of Project Settings.


#include "PoliceCharacter.h"
#include"Components/BoxComponent.h"
#include"Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include"Perception/AIPerceptionComponent.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Kismet/GameplayStatics.h"

APoliceCharacter::APoliceCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bIsAttacking = false;
	//
	CurrentWeapon = EWeaponType::Baton;	
	WalkSpeed = 650.0f;	// more faster than cultist
	//
	AttackCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackCollision"));
	AttackCollision->SetupAttachment(GetMesh(), TEXT("WeaponSocket")); // 소켓에 위치
	AttackCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StimulusComponent = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSourceComponent"));
	StimulusComponent->RegisterForSense(TSubclassOf<UAISense>(UAISense_Sight::StaticClass()));
	StimulusComponent->RegisterWithPerceptionSystem();

	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
}

void APoliceCharacter::BeginPlay()
{
	Super::BeginPlay();

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
}


void APoliceCharacter::StartAttack()
{
	if (bIsAttacking || !AttackMontage) return; 

	UE_LOG(LogTemp, Warning, TEXT("StartAttack Started!"));

	bIsAttacking = true; 

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{

		AnimInstance->Montage_Play(AttackMontage);
		float MontageDuration = AttackMontage->GetPlayLength();

		GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::OnAttackEnd, MontageDuration, false);
	}
}

void APoliceCharacter::OnAttackEnd()
{
	bIsAttacking = false;  
}




void APoliceCharacter::WeaponAttack()
{
	if (CurrentWeapon == EWeaponType::Baton)
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack with Baton"));
		if (AttackMontage)
		{
			PlayAnimMontage(AttackMontage);
		}
	}
	else if (CurrentWeapon == EWeaponType::Pistol)
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack with Pistol"));
		if (AttackMontage)
		{
			PlayAnimMontage(AttackMontage);
		}
	}
	if (CurrentWeapon == EWeaponType::Taser)
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack with Taser"));
		if (AttackMontage)
		{
			PlayAnimMontage(AttackMontage);
		}
	}
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
