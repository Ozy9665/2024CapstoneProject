// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MySocketActor.h"
#include "MySocketCultistActor.h"
#include "MySocketPoliceActor.h"
#include "MyGameInstance.h"
#include "MyNetworkManagerActor.generated.h"

UCLASS()
class CULT_API AMyNetworkManagerActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMyNetworkManagerActor();
	~AMyNetworkManagerActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	SOCKET CanConnectToServer(const FString& ServerIP, int32 ServerPort);
	void CheckAndSpawnActor();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
