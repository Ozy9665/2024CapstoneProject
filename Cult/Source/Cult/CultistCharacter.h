// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include"GameFramework/SpringArmComponent.h"
#include "BaseCharacter.h"
#include "Altar.h"
#include "RitualPerformer.h"
#include "CultistCharacter.generated.h"


// Ability Type
UENUM(BlueprintType)
enum class ESpecialAbility : uint8
{
	Vision UMETA(DisplayName = "Vision"),
	Healing UMETA(DisplayName = "Healing"),
	Rolling UMETA(DisplayName = "Rolling")
};




UCLASS()
class CULT_API ACultistCharacter : public ABaseCharacter, public RitualPerformer
{
	GENERATED_BODY()
	
public:
	ACultistCharacter();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnCamera(float Value);
	void LookUpCamera(float Value);
	void ToggleCrouch();

	void StartRitual() override;
	void StopRitual() override;

	// 의식중 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ritual")
	bool bIsPerformingRitual = false;
	
	// 현재 제단
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual")
	class AAltar* CurrentAltar = nullptr;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual")
	float RitualProgress = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual")
	float RitualSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class USpringArmComponent* SpringArmComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities")
	ESpecialAbility SpecialAbility;






	FTimerHandle RitualTimerHandle;

	// ==== Func ====
	virtual void UseAbility() PURE_VIRTUAL(ACultistCharacter::UseAbility, );

	// Gauge Up
	UFUNCTION(BlueprintCallable, Category="Ritual")
	void PerformRitual();

	void SetCurrentAltar(AAltar* Altar);	// 현재 제단 설정
	
	// 데미지 처리
	virtual void TakeDamage(float DamageAmount) override;
};
