// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CultistCharacter.h"
#include "BuildingBlockComponent.h"
#include "BaseCharacter.h"
#include "Bullet.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Particles/ParticleSystemComponent.h"
#include"GameFramework/Character.h"
#include"GameFramework/SpringArmComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "PoliceDog.h"
#include "PoliceCharacter.generated.h"


class AMySocketPoliceActor;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AimPitch;



	// Combat
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category ="Weapon")
	EWeaponType CurrentWeapon;  
	UPROPERTY(EditAnywhere, BlueprintReadwrite, Category="Combat")
	class UBoxComponent* AttackCollision;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	class UAnimMontage* AttackMontage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	float AttackDamage = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float bulletDamage = 30.0f;
	UPROPERTY(EditDefaultsOnly, Category="Camera")
	float AimFOV = 60.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float DefaultFOV = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	bool bIsAiming = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	float AttackPushForce = 600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	bool bIsShooting = false;

	// 들쳐매기
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class ACultistCharacter* CarryingTarget;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CarryCheckDistance = 100.0f;

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


	// UI
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UUserWidget> CrosshairWidgetClass;

	UUserWidget* CrosshairWidget;


	// =========Func=========
	
	// Movement
	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnCamera(float Value);
	void LookUpCamera(float Value);
	void ToggleCrouch();
	void TurnCharacter();
	FVector MovementInput;

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
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SwitchWeaponEnd();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void FireTaser();
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void EndFireTaser();


	void SetCoolTimeDone();
	
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ShootPistol();
	void UpdateWeaponVisibility();

	UPROPERTY(EditAnywhere, Category = "Combat")
	class UAnimMontage* BatonAttackMontage;

	// Attack Hit
	UFUNCTION(BlueprintCallable, Category="Combat")
	void OnAttackHit(AActor* HitActor);
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void OnShootPistol(const FHitResult& HitResult);
	
	// Particle

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UParticleSystem* ImpactParticle;
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UNiagaraSystem* NG_ImpactParticle;
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UNiagaraSystem* MuzzleImpactParticle;
	void SpawnImpactEffect(FVector ImpactLocation);
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UParticleSystemComponent* ImpactParticleComp;
	bool bHit, bTaserHit;
	FHitResult ParticleResult;

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void VaultStart();
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void VaultEnd();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bIsSwitchingWeapon = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool IsPakour = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bIsNearEnoughToPakour = false;


	FTimerHandle AttackTimerHandle;
	FTimerHandle SwitchWeaponHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	bool bIsAttacking;
	float fCoolTime = 1.5f;
	bool bIsCoolTime = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAIPerceptionStimuliSourceComponent* StimulusComponent;

	// Interaction
	void TryCarry();
	void TryPickUp();
	void TryConfine(AActor* ConfineTarget);
	ACultistCharacter* CarriedCharacter;	// 들고있는 신도

	void TestCollapse();	// Y키
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	AActor* TestTargetActor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	UBuildingBlockComponent* TestTargetBlock;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	float ImpulseAmount = 1200.0f;

	// PoliceDog
	UPROPERTY(EditDefaultsOnly, Category = "PoliceDog")
	TSubclassOf<APoliceDog> PoliceDogClass;
	UPROPERTY(EditDefaultsOnly, Category = "PoliceDog")
	APoliceDog* PoliceDogInstance = nullptr;

	int my_ID = -1;
	UFUNCTION(BlueprintPure)
	static AMySocketPoliceActor* GetMySocketActor();
};
