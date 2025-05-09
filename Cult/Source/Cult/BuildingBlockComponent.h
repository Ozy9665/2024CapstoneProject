// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "BuildingBlockComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CULT_API UBuildingBlockComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UBuildingBlockComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
	float Mass = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
	float MaxImpulseThreshold = 1000.0f;

	// °¨¼è
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
	float AttenuationCoefficient = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
	float CurrentImpulse = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
	bool bCollapsed = false;

	// ÀÌ¿ôºí·Ï
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Destruction")
	TArray<UBuildingBlockComponent*> ConnectedBlocks;

	UFUNCTION(BlueprintCallable, Category="Destruction")
	void ReceiveImpulse(float Impulse, FVector Direction);

	UFUNCTION(BlueprintCallable, Category = "Destruction")
	void Collapse();


protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void PropagateImpulse(float Impulse, FVector Direction);
		
};
