// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include"GameFramework/Character.h"
#include"GameFramework/SpringArmComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "PoliceCharacter.generated.h"

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	Baton UMETA(DisplayName="Baton"),
	Pistol UMETA(DisplayName="Pistol"),
	Taser UMETA(DisplayName="Taser")
};

UCLASS()
class CULT_API APoliceCharacter : public ABaseCharacter
{
	GENERATED_BODY()
	
public:
	APoliceCharacter();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime)override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	


	UPROPERTY(EditAnywhere, Category = "Combat")
	float AttackRange = 150.0f;




	// Combat
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Weapon")
	EWeaponType CurrentWeapon;  
	UPROPERTY(EditAnywhere, BlueprintReadwrite, Category="Combat")
	class UBoxComponent* AttackCollision;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	class UAnimMontage* AttackMontage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	float AttackDamage = 20.0f;
	//	- Mesh
	UPROPERTY(VisibleAnywhere, Category="Weapon")
	UStaticMeshComponent* BatonMesh;
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	UStaticMeshComponent* PistolMesh;
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	UStaticMeshComponent* TaserMesh;


	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class USpringArmComponent* SpringArmComp;



	// =========Func=========
	
	// Movement
	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnCamera(float Value);
	void LookUpCamera(float Value);
	void ToggleCrouch();
	void TurnCharacter();

	// Attack
	//UFUNCTION(BlueprintCallable, Category="Combat")	
	//void WeaponAttack();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void CheckBatonAttack();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void StartAttack();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void EndAttack();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SwitchWeapon();
	void UpdateWeaponVisibility();

	UPROPERTY(EditAnywhere, Category = "Combat")
	class UAnimMontage* BatonAttackMontage;

	// Attack Hit
	UFUNCTION(BlueprintCallable, Category="Combat")
	void OnAttackHit();
	



	FTimerHandle AttackTimerHandle;

	bool bIsAttacking;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAIPerceptionStimuliSourceComponent* StimulusComponent;
};
