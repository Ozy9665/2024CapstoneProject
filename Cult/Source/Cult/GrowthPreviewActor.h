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
	UStaticMeshComponent* PreviewMesh;
	
	UPROPERTY(EditDefaultsOnly)
	UMaterialInterface* ValidMaterial;
	UPROPERTY(EditDefaultsOnly)
	UMaterialInterface* InvalidMaterial;

	UPROPERTY(EditDefaultsOnly)
	UMaterialInstanceDynamic* DynamicMaterial;

	FLinearColor ValidColor = FLinearColor(0.f, 1.f, 0.f, 0.5f);	// 반투명 초록
	FLinearColor InvalidColor = FLinearColor(1.f, 0.f, 0.f, 0.5f);	// 반투명 빨강

	bool bIsPlacementValid = true;

};
