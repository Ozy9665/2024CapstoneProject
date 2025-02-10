// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MySocketActor.h"
#include "MySocketClientActor.h"
#include "AMyNetworkManagerActor.generated.h"

UCLASS()
class SPAWNACTOR_API AAMyNetworkManagerActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAMyNetworkManagerActor();
	~AAMyNetworkManagerActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	bool CanConnectToServer(const FString& ServerIP, int32 ServerPort);
	void CheckAndSpawnActor();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
