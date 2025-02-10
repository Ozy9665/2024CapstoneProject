// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RitualPerformer.generated.h"


UINTERFACE(Blueprintable)
class URitualPerformer : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class CULT_API IRitualPerformer
{
	GENERATED_BODY()

public:
	virtual void StartRitual() = 0;
	virtual void StopRitual() = 0;
};
