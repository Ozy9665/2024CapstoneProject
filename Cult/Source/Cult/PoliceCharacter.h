// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "PoliceCharacter.generated.h"

	// ���� Ÿ�� enum
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

	// ���� ����
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	EWeaponType CurrentWeapon;

	// ����
	virtual void Attack() override;

	// ü��
	void ArrestTarget(class ABaseCharacter* Target);
};
