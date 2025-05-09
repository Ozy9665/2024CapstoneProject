// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BuildingBlock.generated.h"

USTRUCT(BlueprintType)
struct FBuildingBlock
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Mass;

	// ������
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Integrity;

	// �ر�����
	UPROPERTY()
	bool bIsCollapsed;

	// �������
	UPROPERTY()
	float AccumulatedImpulse;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<int32> ConnectedIndices;
	
	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	FBuildingBlock()
		: Position(FVector::ZeroVector)
		, Mass(100.f)
		, Integrity(1.f)
		, bIsCollapsed(false)
		, AccumulatedImpulse(0.f)
	{}
};

