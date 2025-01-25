// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
ABaseCharacter::ABaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// 기본 체력 및 속도, 상태 설정
	Health = 100.0f;
	MovementSpeed = 600.0f;
	CharacterState = ECharacterState::Normal;
}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// 캐릭터 이동 처리
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

// 피해 처리
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

// 상태설정
void ABaseCharacter::SetCharacterState(ECharacterState NewState)
{
	CharacterState = NewState;
}

// 생존 확인
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

