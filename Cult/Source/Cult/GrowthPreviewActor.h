// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrowthPreviewActor.generated.h"

UCLASS()
class CULT_API AGrowthPreviewActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AGrowthPreviewActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void UpdatePreviewLocation(const FVector& NewLocation);
	void SetValidPlacement(bool bIsValid);

private:
	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* PreviewMesh;
	
	UPROPERTY(EditDefaultsOnly)
	class UMaterialInstanceDynamic* DynamicMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Material")
	FLinearColor ValidColor = FLinearColor::Green;

	UPROPERTY(EditDefaultsOnly, Category = "Material")
	FLinearColor InvalidColor = FLinearColor::Red;



	bool bIsPlacementValid = true;

};
