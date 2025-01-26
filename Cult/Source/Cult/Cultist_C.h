// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CultistCharacter.h"
#include "Cultist_C.generated.h"


UCLASS()
class CULT_API ACultist_C : public ACultistCharacter
{
	GENERATED_BODY()
	
public:
	ACultist_C();
	
	virtual void UseAbility() override;
};
