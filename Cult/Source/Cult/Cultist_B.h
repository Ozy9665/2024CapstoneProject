// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CultistCharacter.h"
#include "Cultist_B.generated.h"

/**
 * 
 */
UCLASS()
class CULT_API ACultist_B : public ACultistCharacter
{
	GENERATED_BODY()

public:
	ACultist_B();


	virtual void UseAbility() override;
};
