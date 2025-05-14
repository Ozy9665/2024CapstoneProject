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

	void SetPreviewLocation(const FVector& Location);
	void SetPreviewValid(bool bIsValid);

private:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* PreviewMesh;
	UPROPERTY(EditDefaultsOnly)
	UMaterialInterface* ValidMaterial;
	UPROPERTY(EditDefaultsOnly)
	UMaterialInterface* InvalidMaterial;

};
