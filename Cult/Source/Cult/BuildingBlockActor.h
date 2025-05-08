// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildingBlockComponent.h"
#include "BuildingBlockActor.generated.h"


UCLASS()
class CULT_API ABuildingBlockActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABuildingBlockActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	void BreakSelf();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UBuildingBlockComponent* BlockComponent;
};
