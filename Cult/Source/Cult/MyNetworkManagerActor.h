// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MySocketActor.h"
#include "MySocketCultistActor.h"
#include "MySocketPoliceActor.h"
#include "Blueprint/UserWidget.h"
#include "MyGameInstance.h"
#include "MyNetworkManagerActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomListUpdated);

UCLASS()
class CULT_API AMyNetworkManagerActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMyNetworkManagerActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	bool CanConnectToServer(const FString& ServerIP, int32 ServerPort);
	void CheckServer();
	void RequestRoomInfo();
	void ProcessRoomInfo(const char* Buffer);
	void ReceiveData();

	UFUNCTION(BlueprintCallable, Category = "Network")
	void SendGameStart(int32 RoomNumber);

	void SpawnActor();


private:
	SOCKET ClientSocket;
	int my_ID = -1;
	int my_Role = -1;

public:		
    UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
    TArray<Froom> rooms;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnRoomListUpdated OnRoomListUpdated;



	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
