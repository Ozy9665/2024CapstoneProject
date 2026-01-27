// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StructManager.generated.h"

USTRUCT()
struct FStructNode
{
	GENERATED_BODY()

	UPROPERTY() AActor* Actor = nullptr;
	UPROPERTY() int32 NodeId = -1;

	// 그래프
	UPROPERTY() int32 LeftId = -1;
	UPROPERTY() int32 RightId = -1;

	// 하중
	UPROPERTY() float CurrentLoad = 1.0f;
	UPROPERTY() float Capacity = 3.0f;
};

UCLASS()
class CULT_API AStructManager : public AActor
{
	GENERATED_BODY()

public:
	AStructManager();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Struct|Tags")
	FName ColumnTag = "Struct_Column";

	// 바닥 기둥 판정 임계값  / 최대높이
	UPROPERTY(EditAnywhere, Category = "Struct|Filter")
	float BaseColumnMinZThreshold = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Struct|Debug")
	bool bDrawDebugGraph = true;

	UPROPERTY(EditAnywhere, Category = "Struct|Debug")
	float DebugDuration = 0.0f; //

	UPROPERTY(EditAnywhere, Category = "Struct|Debug")
	float DebugLineThickness = 4.0f;

private:
	UPROPERTY() TArray<FStructNode> Nodes;

	void BuildBaseColumns();
	void LinkNeighborsByX();
	void DrawDebug();
};
