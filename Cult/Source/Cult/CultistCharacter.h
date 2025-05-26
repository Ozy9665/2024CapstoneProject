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

class AMySocketCultistActor;

// Ability Type
UENUM(BlueprintType)
enum class ESpecialAbility : uint8
{
	Vision UMETA(DisplayName = "Growth"),
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
	float RitualSpeed = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class USpringArmComponent* SpringArmComp;



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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bIsDead = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bIsPakour = false;
	FTimerHandle ReviveTimerHandle;
	FTimerHandle HitByAttackTH;
	FTimerHandle DisableTimerHandle;
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

	UFUNCTION(BlueprintCallable, Category = "Health") 
	bool IsInactive()const;

	FTimerHandle RitualTimerHandle;

	// ==== Func ====
	virtual void UseAbility() PURE_VIRTUAL(ACultistCharacter::UseAbility, );

	// Gauge Up
	UFUNCTION(BlueprintCallable, Category="Ritual")
	void PerformRitual();

	void SetCurrentAltar(AAltar* Altar);	// 현재 제단 설정
	
	// 데미지 처리
	virtual void TakeDamage(float DamageAmount) override;
	void OnHitbyBaton(const FVector& AttackerLocation, float AttackDamage);
	// Interaction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bIsConfined = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bIsBeingCarried = false;

	// 피격효과 위젯 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUserWidget> BloodWidgetClass;
	UPROPERTY()
	UUserWidget* BloodEffectWidget;
	// 바인딩할 애니메이션
	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* HitBloodEffect;


	// Ability
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	ESpecialAbility SpecialAbility;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class AGrowthPreviewActor> GrowthPreviewActorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<class ATreeObstacleActor> TreeObstacleActorClass;

	UPROPERTY()
	AGrowthPreviewActor* SpawnedPreviewActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	class UStaticMeshComponent* RangeVisualizer;

	UPROPERTY(EditDefaultsOnly, Category ="Abilities")
	float PreviewTraceDistance = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	bool bCanPlace = false;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	float MaxPlacementDistance = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TEnumAsByte<ECollisionChannel> PlacementCheckChannel = ECC_WorldStatic;


	UFUNCTION(BlueprintCallable, Category="Abilities")
	void StartPreviewPlacement();

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void UpdatePreviewPlacement();

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void ConfirmPlacement();

	bool bTreeSkillReady = true;
	FTimerHandle TreeSkillCooldownHandle;
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void ResetTreeSkillCooldown();
	UPROPERTY(EditAnywhere, Category = "Abilities")
	float TreeSkillCooldownTime = 3.f;

	int my_ID = -1;
	int GetPlayerID() const;
	void SendDisableToServer();
};
