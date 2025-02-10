// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RitualPerformer.h"
#include "BaseCharacter.h"

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
class CULT_API ACultistCharacter : public ABaseCharacter
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

	// void StartRitual() override;
	// void StopRitual() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ritual")
	bool bIsPerformingRitual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual")
	float RitualProgress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual")
	float RitualSpeed;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities")
	ESpecialAbility SpecialAbility;

	// ==== Func ====
	virtual void UseAbility() PURE_VIRTUAL(ACultistCharacter::UseAbility, );

	// Gauge Up
	UFUNCTION(BlueprintCallable, Category="Ritual")
	void PerformRitual();
};
