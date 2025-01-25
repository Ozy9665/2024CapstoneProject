// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
ABaseCharacter::ABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// �⺻ ü�� �� �ӵ�, ���� ����
	Health = 100.0f;
	MovementSpeed = 600.0f;
	CharacterState = ECharacterState::Normal;
}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// ĳ���� �̵� ó��
void ABaseCharacter::MoveForward(float Value)
{
	if (Value != 0.0f && CharacterState == ECharacterState::Normal)
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void ABaseCharacter::MoveRight(float Value)
{
	if (Value != 0.0f && CharacterState == ECharacterState::Normal)
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

// ���� ó��
void ABaseCharacter::TakeDamage(float DamageAmount)
{
	Health -= DamageAmount;
	if (Health <= 0.0f)
	{
		SetCharacterState(ECharacterState::Dead);
	}
}

void ABaseCharacter::Attack() {

}

// ���¼���
void ABaseCharacter::SetCharacterState(ECharacterState NewState)
{
	CharacterState = NewState;
}

// ���� Ȯ��
bool ABaseCharacter::IsAlive() const
{
	return Health > 0;
}



// Called every frame
void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 
	PlayerInputComponent->BindAxis("MoveForward", this, &ABaseCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ABaseCharacter::MoveRight);
}

