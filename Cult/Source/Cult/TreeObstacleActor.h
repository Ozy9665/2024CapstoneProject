// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "GameFramework/Actor.h"
#include "TreeObstacleActor.generated.h"

UCLASS()
class CULT_API ATreeObstacleActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATreeObstacleActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* TreeMesh;


	// Growth
	UPROPERTY(EditAnywhere, Category = "Growth")
	float GrowHeight = 200.f;

	UPROPERTY(EditAnywhere, Category = "Growth")
	float GrowTime = 1.0f;

	FVector InitialLocation;
	FVector TargetLocation;

	float ElapsedTime = 0.f;
	bool bIsGrowing = true;

	// Spline
	UPROPERTY(VisibleAnywhere, Category = "Branches", meta = (AllowPrivateAccess = "true"))
	USplineComponent* Spline;

	UPROPERTY(EditAnywhere)
	UStaticMesh* BranchMesh;

	TArray<USplineMeshComponent*> SpawnedBranches;

	UFUNCTION()
	void SetupBranches();
};
