// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "GameFramework/Actor.h"
#include "PerceptionActor.generated.h"

UCLASS()
class CULT_API APerceptionActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APerceptionActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere)
	UAIPerceptionComponent* PerceptionComponent;

	UPROPERTY(VisibleAnywhere)
	UAISenseConfig_Sight* SightConfig;
};
