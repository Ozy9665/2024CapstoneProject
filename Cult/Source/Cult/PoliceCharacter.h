// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "Bullet.h"
#include "Particles/ParticleSystemComponent.h"
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


UENUM(BlueprintType)
enum class EVaultingType : uint8
{
	OneHandVault UMETA(DisplayName = "OneHandVault"),
	TwoHandVault UMETA(DisplayName = "TwoHandVault"),
	FrontFlip UMETA(DisplayName = "FrontFlip")
};



UCLASS()
class CULT_API APoliceCharacter : public ABaseCharacter
{
	GENERATED_BODY()
	
public:
	APoliceCharacter(const FObjectInitializer& ObjectInitializer);

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
	UPROPERTY(EditDefaultsOnly, Category="Camera")
	float AimFOV = 60.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float DefaultFOV = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	bool bIsAiming = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	float AttackPushForce = 1200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	bool bIsShooting = false;

	//	- Mesh
	UPROPERTY(VisibleAnywhere, Category="Weapon")
	UStaticMeshComponent* BatonMesh;
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	UStaticMeshComponent* PistolMesh;
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	UStaticMeshComponent* TaserMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	TSubclassOf<class ABullet> BulletClass;
	UPROPERTY(EditAnywhere, Category="Weapon")
	TSubclassOf<class ABullet> TaserProjectileClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	bool bIsUsingTaser = false;
	UPROPERTY(EditDefaultsOnly, Category="Weapon")
	USceneComponent* MuzzleLocation;


	UPROPERTY()
	class UCameraComponent* CameraComp;

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

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void OnAimPressed();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void StartAiming();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void StopAiming();
	//void Shoot();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	EVaultingType CurrentVaultType;


	// Attack
	//UFUNCTION(BlueprintCallable, Category="Combat")	
	//void WeaponAttack();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void BatonAttack();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void StartAttack();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void EndAttack();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void EndPistolShoot();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SwitchWeapon();
	
	void SetCoolTimeDone();
	
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ShootPistol();
	void UpdateWeaponVisibility();

	UPROPERTY(EditAnywhere, Category = "Combat")
	class UAnimMontage* BatonAttackMontage;

	// Attack Hit
	UFUNCTION(BlueprintCallable, Category="Combat")
	void OnAttackHit();
	
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UParticleSystem* ImpactParticle;
	void SpawnImpactEffect(FVector ImpactLocation);
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UParticleSystemComponent* ImpactParticleComp;
	bool bHit;
	FVector ImpactLoc;

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void VaultStart();
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void VaultEnd();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool IsPakour = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bIsNearEnoughToPakour = false;


	FTimerHandle AttackTimerHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	bool bIsAttacking;
	float fCoolTime = 1.5f;
	bool bIsCoolTime = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAIPerceptionStimuliSourceComponent* StimulusComponent;
};
