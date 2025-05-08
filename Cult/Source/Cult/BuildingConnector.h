// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildingBlockComponent.h"
#include "BuildingConnector.generated.h"

UCLASS()
class CULT_API ABuildingConnector : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABuildingConnector();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Scan")
	float ConnectionRadius = 120.f;

	UPROPERTY(EditAnywhere, Category = "Scan")
	TSubclassOf<AActor> BlockClass;

	UPROPERTY(EditAnywhere, Category="Connection")
	FName TargetBuildingID = "Sun_Kak1";

	UPROPERTY(EditAnywhere, Category="Connection")
	float ConnectionRadios = 300.0f;

	void ScanAndConnectBlocks();

	TArray<AActor*> FoundBlocks;


public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
