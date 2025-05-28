// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "GameFramework/Actor.h"
#include "TreeObstacleActor.generated.h"

UENUM()
enum class ETreeGrowState : uint8
{
	GrowingTrunk,
	GrowingBranches,
	Done
};

USTRUCT()
struct FBranchMeshGroup
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<USplineMeshComponent*> Meshes;
};


USTRUCT()
struct FBranchData
{
	GENERATED_BODY()

	UPROPERTY()
	USplineComponent* Spline;

	UPROPERTY()
	TArray<USplineMeshComponent*> Meshes;

	UPROPERTY()
	float CurrentLength = 0.f;

	UPROPERTY()
	int32 Depth = 0; // 뿌리에서 얼마나 멀리 떨어졌는지

	UPROPERTY()
	FVector InitialDirection = FVector::ZeroVector;

	// 이후 자식 분기들을 위해
	TArray<FBranchData> Children;
};



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
	UPROPERTY(EditAnywhere)
	UStaticMesh* BranchMesh;

	UPROPERTY(EditAnywhere, Category = "Branches")
	float BranchGrowSpeed = 500.f;

	UPROPERTY(EditAnywhere, Category = "Branches")
	float BranchMaxLength = 3000.f;

	TArray<USplineComponent*> BranchSplines;
	TArray<USplineMeshComponent*> BranchMeshes;
	TArray<float> CurrentBranchLengths;

	TArray<USplineMeshComponent*> SpawnedBranches;
	ETreeGrowState GrowState = ETreeGrowState::GrowingTrunk;

	UPROPERTY()
	TArray<FBranchMeshGroup> BranchMeshGroups;

	UPROPERTY()
	TArray<FBranchData> AllBranches;

	void CreateBranch(const FVector& StartLocation, const FRotator& Rotation, int32 Depth);

	UPROPERTY(EditAnywhere, Category = "Branches")
	int32 MaxBranchDepth = 2;
	//UPROPERTY(EditAnywhere, Category = "Branches")
	//int32 NumFirstBranches = 10;

	// CoolTime
	FTimerHandle CooldownHandle;




	UFUNCTION()
	void SetupBranches();
};
