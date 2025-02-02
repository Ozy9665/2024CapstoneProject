// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CultistCharacter.h"
#include "Cultist_A.generated.h"


UCLASS()
class CULT_API ACultist_A : public ACultistCharacter
{
	GENERATED_BODY()

public:
	ACultist_A();


	virtual void UseAbility() override;

};
