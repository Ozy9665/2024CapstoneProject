// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MySocketActor.h"
#include "Blueprint/UserWidget.h"
#include "MyGameInstance.h"
#include "MyNetworkManagerActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomListUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameStartConfirmed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameStartUnConfirmed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLoginSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLoginFailed, uint8, ReasonCode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FIdIsExist);	
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FIdIsNotExist);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSignUpSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSignUpFailed);

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

private:
	SOCKET ClientSocket;
	int my_ID = -1;
	UMyGameInstance* GI;


public:		
    UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
    TArray<Froom> rooms;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnRoomListUpdated OnRoomListUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnGameStartConfirmed OnGameStartConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnGameStartConfirmed OnGameStartUnConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FLoginSuccess OnLoginSuccess;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FLoginFailed OnLoginFailed;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FIdIsExist OnIdIsExist;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FIdIsNotExist OnIdIsNotExist;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FSignUpSuccess OnSignUpSuccess;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FSignUpFailed OnSignUpFailed;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool CanConnectToServer(const FString& ServerIP, int32 ServerPort);
	void CheckServer();
	void ReceiveData();

	UFUNCTION(BlueprintCallable, Category = "Network")
	void TryLogin();

	UFUNCTION(BlueprintCallable, Category = "Network")
	void CheckIDValidation();

	UFUNCTION(BlueprintCallable, Category = "Network")
	void TrySignUp();

	UFUNCTION(BlueprintCallable, Category = "Network")
	void RequestRoomInfo();
	void ProcessRoomInfo(const char* Buffer);

	UFUNCTION(BlueprintCallable, Category = "Network")
	void SendEnterPacket();
	void RequestRitualData();
	void ProcessRitualData(const char* Buffer);
};
