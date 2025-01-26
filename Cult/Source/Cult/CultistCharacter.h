// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "CultistCharacter.generated.h"

// Ư���ɷ� Ÿ��
UENUM(BlueprintType)
enum class ESpecialAbility : uint8
{
	Vision UMETA(DisplayName="Vision"),
	Healing UMETA(DisplayName="Healing"),
	Rolling UMETA(DisplayName="Rolling")
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
	// �ɷ�. �������� �ٽ� ����
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	ESpecialAbility SpecialAbility;

	// �ǽ� ����
	void PerformRitual();

	// �ɷ� ����
	virtual void UseAbility() PURE_VIRTUAL(ACultistCharacter::UseSpecialAbility, );

};
