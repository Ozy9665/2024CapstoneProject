// Fill out your copyright notice in the Description page of Project Settings.


#include "CultistCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/ProgressBar.h"
#include "Components/InputComponent.h"


ACultistCharacter::ACultistCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	WalkSpeed = 600.0f;
	

	Health = 100.0f;
	CurrentHealth = 100.0f;
	bIsStunned = false;
	bIsAlreadyStunned = false;

	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	

	bIsPerformingRitual = false;
	RitualProgress = 0.0f;
	RitualSpeed = 10.0f; // �ʴ� �����ӵ�

	// �ǽ� ���� �۾� ���൵
	TaskRitualProgress = 0.0f;
	TaskRitualSpeed = 20.0f;


}

void ACultistCharacter::BeginPlay()
{
	Super::BeginPlay();
	// ��Ʈ�ѷ� yaw ȸ�� false
	bUseControllerRotationYaw = false;
	// �Է� �������� ȸ��
	GetCharacterMovement()->bOrientRotationToMovement = true;
	// ȸ���ӵ�
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	
	// ���������̱�
	GetCharacterMovement()->GroundFriction = 0.1f; 

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
	if (!TaskRitualWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("TaskRitualWidgetClass is NULL! Make sure it's set in Blueprint."));
		return;
	}
	if (TaskRitualWidgetClass)
	{
		TaskRitualWidget = CreateWidget<UUserWidget>(GetWorld(), TaskRitualWidgetClass);
		if (TaskRitualWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("TaskRitualWidget Created"));
			TaskRitualWidget->AddToViewport();
			TaskRitualWidget->SetVisibility(ESlateVisibility::Hidden);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create widget"));
		}
	}


}

void ACultistCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &ACultistCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ACultistCharacter::MoveRight);
	PlayerInputComponent->BindAction("PerformRitual", IE_Pressed, this, &ACultistCharacter::StartRitual);


	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ACultistCharacter::ToggleCrouch);
}

void ACultistCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		FRotator ControlRotation = GetControlRotation();
		FRotator YawRotation(0, ControlRotation.Yaw, 0);

		FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void ACultistCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		FRotator ControlRotation = GetControlRotation();
		FRotator YawRotation(0, ControlRotation.Yaw, 9);

		FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
}

void ACultistCharacter::ToggleCrouch()
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

void ACultistCharacter::PerformRitual()
{
	if (!bIsPerformingRitual) return;

	TaskRitualProgress += TaskRitualSpeed * 0.1f; // �ǽ� �۾� ���൵
	
	if (TaskRitualWidget)
	{
		UProgressBar* ProgressBar = Cast<UProgressBar>(TaskRitualWidget->GetWidgetFromName(TEXT("RitualProgressBar")));
		if (ProgressBar)
		{
			ProgressBar->SetPercent(TaskRitualProgress / 100.0f);
			UE_LOG(LogTemp, Warning, TEXT("TaskProgressBar Exist"));
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("Performing Ritual..."));
	UE_LOG(LogTemp, Warning, TEXT("TaskRitualProgress: %f"), TaskRitualProgress);
	if (TaskRitualProgress >= 100.0f)
	{
		CurrentAltar->IncreaseRitualGauge();
		StopRitual();
	}


	// Check 100%
	//if (RitualProgress >= 100.0f)
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("Ritual Completed!"));
	//}


}

void ACultistCharacter::StartRitual()
{
	if (bIsPerformingRitual)
	{
		UE_LOG(LogTemp, Warning, TEXT("Already Performing.."));
		return;
	}
	if (!CurrentAltar) { 
		UE_LOG(LogTemp, Warning, TEXT("No altar Here!"));
		return;
	}

	bIsPerformingRitual = true;
	TaskRitualProgress = 0.0f;
	if (TaskRitualWidget)
	{
		TaskRitualWidget->SetVisibility(ESlateVisibility::Visible);
	}
	
	GetWorld()->GetTimerManager().SetTimer(RitualTimerHandle, this, &ACultistCharacter::PerformRitual, 0.1f, true);
	UE_LOG(LogTemp, Warning, TEXT("Ritual Started"));

	GetCharacterMovement()->DisableMovement();

	// Animation
}

// �ߴ� x, �Ϸ�ó��
void ACultistCharacter::StopRitual()
{
	if (!bIsPerformingRitual)return;

	bIsPerformingRitual = false;
	GetWorld()->GetTimerManager().ClearTimer(RitualTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(TaskRitualTimerHandle);

	UE_LOG(LogTemp, Warning, TEXT("Ritual Stopped"));
	RitualProgress += RitualSpeed;	// ��ü �ǽİ�����

	if (TaskRitualWidget)
	{
		TaskRitualWidget->SetVisibility(ESlateVisibility::Hidden);
		TaskRitualProgress = 0.0f;
	}

	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ACultistCharacter::CancelRitual()
{
	UE_LOG(LogTemp, Warning, TEXT("Ritual Canceled"));

	bIsPerformingRitual = false;
	GetWorld()->GetTimerManager().ClearTimer(RitualTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(TaskRitualTimerHandle);

	if (TaskRitualWidget)
	{
		TaskRitualWidget->SetVisibility(ESlateVisibility::Hidden);
		TaskRitualProgress = 0.0f;
	}

	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ACultistCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ACultistCharacter::SetCurrentAltar(AAltar* Altar)
{
	CurrentAltar = Altar;
}

void ACultistCharacter::TakeDamage(float DamageAmount)
{
	if (bIsStunned)return;
	bIsHitByAnAttack = true;
	
	if (bIsPerformingRitual)
	{
		CancelRitual();
	}


	// ��� �̵��Ұ�
	GetCharacterMovement()->MaxWalkSpeed = 0.0f;
	GetCharacterMovement()->GroundFriction = 0.1f; // ���������̱�
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// ��� ��ȭ
	//GetCharacterMovement()->MaxWalkSpeed *= 0.4f;


	// �ǰ� ���׼� Ÿ�̸�
	GetWorld()->GetTimerManager().SetTimer(HitByAttackTH, this, &ACultistCharacter::GottaRun, 0.8f, false);
	
	// ������ ó��
	CurrentHealth -= DamageAmount;
	UE_LOG(LogTemp, Warning, TEXT("HP : %f Now"), CurrentHealth);

	if (CurrentHealth <= 0)
	{
		if (bIsAlreadyStunned)
		{
			Die();
		}
		else
		{
			Stun();
		}
	}
}

void ACultistCharacter::GottaRun()
{
	UE_LOG(LogTemp, Warning, TEXT("Cultist Gotta Run"));
	bIsHitByAnAttack = false;

	//GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->MaxWalkSpeed = 600.0f;	// �ӵ� ����
	GetCharacterMovement()->GroundFriction = 8.0f; // ������ ����
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void ACultistCharacter::Die()
{
	UE_LOG(LogTemp, Warning, TEXT("Died."));
}

void ACultistCharacter::Stun()
{
	UE_LOG(LogTemp, Warning, TEXT("Got Stunned"));
	bIsStunned = true;

	GetCharacterMovement()->DisableMovement();

	//AI ����
	GetCharacterMovement()->MaxWalkSpeed = 0.0f;

	// ĸ���浹x
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);



	IntoRagdoll();

	// ���� �ð� �� ���
	GetWorld()->GetTimerManager().SetTimer(ReviveTimerHandle, this, &ACultistCharacter::Revive, 10.0f, false);
}

void ACultistCharacter::Revive()
{
	if (!bIsStunned)return;

	bIsStunned = false;
	bIsAlreadyStunned = true;

	// �����ù�Off, �浹 ����
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
	GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	
	// �޽�-ĸ�� ����
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	// �޽� ��ġ, ȸ�� �ʱ�ȭ	GetActorRotation().Yaw
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetRelativeLocation(FVector(0, 0, -100));
	// ��ġ���� Ȯ��
	UE_LOG(LogTemp, Warning, TEXT("Mesh Re;ativeLocation: %s"), *GetMesh()->GetRelativeLocation().ToString());

	// �ִϸ��̼� ����
	GetMesh()->bPauseAnims = false;
	GetMesh()->bNoSkeletonUpdate = false;
	
	// ���� ü�� �������·� ����
	Health = 50.0f;

	// �̵� ������ ���·� ����
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->MaxWalkSpeed = 550.0f;
	UE_LOG(LogTemp, Warning, TEXT("Revive."));
}

void ACultistCharacter::IntoRagdoll()
{
	// Ragdoll ȿ��

	// - ���� ��ȯ
	GetMesh()->SetSimulatePhysics(true);	// �����ù�On
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->bBlendPhysics = true;
	GetMesh()->WakeAllRigidBodies();
	
	// �ִϸ��̼� pause
	FTimerHandle PauseHandle;
	GetWorld()->GetTimerManager().SetTimer(PauseHandle, [this]()
		{
			GetMesh()->bPauseAnims = true;
			GetMesh()->bNoSkeletonUpdate = true;
		}, 0.05f, false);
}

void ACultistCharacter::TakeMeleeDamage(float DamageAmount)
{
	TakeDamage(DamageAmount);
	// ����
}

void ACultistCharacter::TakePistolDamage(float DamageAmount)
{
	TakeDamage(DamageAmount);
	// �ǰ�

}

void ACultistCharacter::GotHitTaser()
{
	// ������ - ��ȿ, �ǰ��� - ��ȿ
	if (bIsStunned)return;
	Stun();
	// ����

}

// �̵� �� �ߴ� but �̵��Ұ��� ����
/*
void ACultistCharacter::MoveForward(float Value)
{
	if (Value != 0.0f) StopRitual();
}*/


void ACultistCharacter::TurnCamera(float Value)
{
	AddControllerYawInput(Value);
}
void ACultistCharacter::LookUpCamera(float Value)
{
	AddControllerPitchInput(Value);
}

