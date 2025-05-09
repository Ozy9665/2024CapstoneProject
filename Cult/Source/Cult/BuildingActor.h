// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildingBlockComponent.h"
#include "BuildingActor.generated.h"

UCLASS()
class CULT_API ABuildingActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABuildingActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Destruction")
	TSubclassOf<UBuildingBlockComponent> BlockComponentClass;

	// 생성할 블록 개수(가로, 세로, 높이)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
	int32 Width = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
	int32 Height = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
	int32 Depth = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
	float BlockSpacing = 110.f;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	// 생성된 블록 참조
	TArray<UBuildingBlockComponent*>Blocks;

	void GenerateBuilding();
	void ConnectNeighbors();
	int32 GetIndex(int32 X, int32 Y, int32 Z) const;
};
