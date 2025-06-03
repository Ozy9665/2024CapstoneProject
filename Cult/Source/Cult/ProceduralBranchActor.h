// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralBranchActor.generated.h"

USTRUCT()
struct FBranchNode
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector> SplinePoints;

	UPROPERTY()
	float RadiusStart = 10.f;

	UPROPERTY()
	float RadiusEnd = 2.f;

	UPROPERTY()
	int32 Depth = 0;

	UPROPERTY()
	FVector Direction = FVector::ForwardVector;

	UPROPERTY()
	FVector StartLocation = FVector::ZeroVector;
};


UCLASS()
class CULT_API AProceduralBranchActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AProceduralBranchActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere)
	UProceduralMeshComponent* ProcMesh;

	UPROPERTY(EditAnywhere)
	int32 MaxDepth = 2;

	UPROPERTY(EditAnywhere)
	int32 BranchSegmentCount = 6;

	UPROPERTY(EditAnywhere)
	float BranchLength = 200.f;

	UPROPERTY(EditAnywhere)
	float BranchAngleRange = 30.f;

	UPROPERTY()
	TArray<FBranchNode> AllBranches;

	UPROPERTY(EditAnywhere)
	bool bIsMainTrunk = true;

	//void CreateBranch(const FVector& Start, const FVector& Direction, int32 Depth);

	//void BuildAllBranches();

	void GenerateCylinderAlongSpline(const TArray<FVector>& SplinePoints, float RadiusStart, float RadiusEnd);
	void GenerateCurvedBranch(float Length = 300.f, float RadiusStart = 12.f, float RadiusEnd = 2.f, int32 Segments = 16);
	FTimerHandle BranchSpawnHandle;
	void SpawnBranches();

};
