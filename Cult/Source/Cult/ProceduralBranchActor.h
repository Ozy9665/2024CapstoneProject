// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralBranchActor.generated.h"

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

	//UFUNCTION(BlueprintCallable)

	void GenerateCurvedBranch(float Length = 300.f, float RadiusStart = 12.f, float RadiusEnd = 2.f, int32 Segments = 16);

private:
	void GenerateCylinderAlongSpline(const TArray<FVector>& SplinePoints, float RadiusStart, float RadiusEnd);
};
