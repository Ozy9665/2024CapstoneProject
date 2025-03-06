// Fill out your copyright notice in the Description page of Project Settings.


#include "PoliceCharacter.h"
#include "GameFramework/Actor.h"
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

APoliceCharacter::APoliceCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bIsAttacking = false;
	//
	CurrentWeapon = EWeaponType::Baton;	
	WalkSpeed = 650.0f;	// more faster than cultist
	// melee 콜리전 설정
	AttackCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackCollision"));
	AttackCollision->SetupAttachment(GetMesh(), TEXT("WeaponSocket")); // 소켓에 위치
	AttackCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StimulusComponent = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSourceComponent"));
	StimulusComponent->RegisterForSense(TSubclassOf<UAISense>(UAISense_Sight::StaticClass()));
	StimulusComponent->RegisterWithPerceptionSystem();


}

void APoliceCharacter::BeginPlay()	// 초기화
{
	Super::BeginPlay();
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);

	SpringArmComp = FindComponentByClass<USpringArmComponent>();
	if (SpringArmComp)
	{
		SpringArmComp->TargetArmLength = 300.0f;
		SpringArmComp->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
		UE_LOG(LogTemp, Warning, TEXT("Set SpringArm"));
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("SpringArm null"));
		SpringArmComp = NewObject<USpringArmComponent>(this);
		if (SpringArmComp)
		{
			SpringArmComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			SpringArmComp->RegisterComponent();
			SpringArmComp->TargetArmLength = 300.0f;
			SpringArmComp->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
			UE_LOG(LogTemp, Warning, TEXT("So make SpringArm"));
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("Still Null SpringArm"));
		}
	}

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
	if (bIsAttacking || !AttackMontage) {
		UE_LOG(LogTemp, Warning, TEXT("Already or NULL"));
		return;
	};

	bIsAttacking = true; 
	if (CurrentWeapon == EWeaponType::Baton)
	{
		UE_LOG(LogTemp, Warning, TEXT("Baton Attack"));

		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

		if (!AnimInstance)
		{
			UE_LOG(LogTemp, Warning, TEXT("AnimaInstance NULL"));
		}

		if (AttackMontage)
		{

			AnimInstance->Montage_Play(AttackMontage);
			UE_LOG(LogTemp, Warning, TEXT("Play Montage"));
			if (AnimInstance->Montage_IsPlaying(AttackMontage))
			{
				UE_LOG(LogTemp, Warning, TEXT("Montage is Playing"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Montage failed to play"));
			}

			// 이동불가
			// 
			// 
			// 판정
			GetWorldTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::CheckBatonAttack, 0.3f, false);

			// 애니메이션 끝나고 공격종료
			float MontageDuration = AttackMontage->GetPlayLength();

			GetWorld()->GetTimerManager().SetTimer(AttackTimerHandle, this, &APoliceCharacter::EndAttack, MontageDuration, false);
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("Animation is NULL"));
		}
	}

}





void APoliceCharacter::CheckBatonAttack()
{
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
	bIsAttacking = false;

	// 이동 가능
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
/*
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
*/