// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "CultistCharacter.generated.h"

// 특수능력 타입
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
	// 능력. 각각에서 다시 지정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
	ESpecialAbility SpecialAbility;

	// 의식 수행
	void PerformRitual();

	// 능력 수행
	virtual void UseAbility() PURE_VIRTUAL(ACultistCharacter::UseSpecialAbility, );

};
