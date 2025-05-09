// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildingBlock.h"
#include "BuildingStructure.generated.h"

UCLASS()
class CULT_API ABuildingStructure : public AActor
{
	GENERATED_BODY()
	
public:	
	ABuildingStructure();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	UPROPERTY()
	TArray<FBuildingBlock> Blocks;

	UFUNCTION()
	void InitializeStructure(int32 Width, int32 Height, int32 Depth);

	// ½Ã°¢È­
	void DebugDrawStructure();

	UFUNCTION(BlueprintCallable)
	void ApplyImpulseToBlock(int32 BlockIndex, const FVector& Impulse);

private:
	// 3d -> index
	int32 GetBlockIndex(int32 X, int32 Y, int32 Z, int32 Width, int32 Height, int32 Depth) const;
};
