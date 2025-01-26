// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "PoliceCharacter.generated.h"

	// 무기 타입 enum
UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	Baton UMETA(DisplayName = "Baton"),
	Pistol UMETA(DisplayName = "Pistol"),
	Taser UMETA(DisplayName = "Taser")
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

	// 현재 무기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	EWeaponType CurrentWeapon;

	// 공격
	virtual void Attack() override;

	// 체포
	void ArrestTarget(class ABaseCharacter* Target);
};
