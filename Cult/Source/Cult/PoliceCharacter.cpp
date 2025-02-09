// Fill out your copyright notice in the Description page of Project Settings.


#include "PoliceCharacter.h"
#include"Components/BoxComponent.h"
#include"Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

APoliceCharacter::APoliceCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	//
	CurrentWeapon = EWeaponType::Baton;	
	WalkSpeed = 650.0f;	// more faster than cultist
	//
	AttackCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("AttackCollision"));
	AttackCollision->SetupAttachment(GetMesh(), TEXT("WeaponSocket")); // 소켓에 위치
	AttackCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APoliceCharacter::BeginPlay()
{
	Super::BeginPlay();
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