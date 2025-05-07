// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Blueprint/UserWidget.h"
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
	void CancelRitual();

	// 의식중 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ritual")
	bool bIsPerformingRitual = false;
	
	// 현재 제단
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual")
	class AAltar* CurrentAltar = nullptr;

	// 현 의식 게이지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual")
	float TaskRitualProgress;
	float TaskRitualSpeed;
	FTimerHandle TaskRitualTimerHandle;

	// UI
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> TaskRitualWidgetClass;
	UUserWidget* TaskRitualWidget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual")
	float RitualProgress = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual")
	float RitualSpeed = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class USpringArmComponent* SpringArmComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities")
	ESpecialAbility SpecialAbility;

	// Damage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float CurrentHealth;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bIsHitByAnAttack=false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bIsStunned;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health")
	bool bIsAlreadyStunned;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bIsFrontFallen = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bIsElectric = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool TurnToStun = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool TurnToGetUp = false;
	FTimerHandle ReviveTimerHandle;
	FTimerHandle HitByAttackTH;
	UFUNCTION(BlueprintCallable, Category="Health")
	void Die();
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Stun();
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Revive();
	UFUNCTION(BlueprintCallable, Category = "Health")
	void TakeMeleeDamage(float DamageAmount);
	UFUNCTION(BlueprintCallable, Category = "Health")
	void TakePistolDamage(float DamageAmount);
	UFUNCTION(BlueprintCallable, Category = "Health")
	void GotHitTaser(AActor* Attacker);
	UFUNCTION(BlueprintCallable, Category = "Health")
	void FallDown();
	UFUNCTION(BlueprintCallable, Category = "Health")
	void GetUp();
	UFUNCTION(BlueprintCallable, Category = "Health")
	void GottaRun();

	UFUNCTION(BlueprintCallable, Category = "Health")
	void IntoRagdoll();

	FTimerHandle RitualTimerHandle;

	// ==== Func ====
	virtual void UseAbility() PURE_VIRTUAL(ACultistCharacter::UseAbility, );

	// Gauge Up
	UFUNCTION(BlueprintCallable, Category="Ritual")
	void PerformRitual();

	void SetCurrentAltar(AAltar* Altar);	// 현재 제단 설정
	
	// 데미지 처리
	virtual void TakeDamage(float DamageAmount) override;

	// Interaction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bIsConfined = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bIsBeingCarried = false;

	int my_ID = -1;
	int GetPlayerID() const;
};
