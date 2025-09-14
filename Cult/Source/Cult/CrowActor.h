// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Components/SphereComponent.h"
#include "CrowActor.generated.h"

UENUM(BlueprintType)
enum class ECrowState : uint8
{
	Idle,
	Patrol,
	Alert
};

UCLASS()
class CULT_API ACrowActor : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ACrowActor();

	void InitCrow(AActor* InOwner, float InLifeTime);
	void EnterAlertState(AActor* PoliceTarget);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// ������Ʈ
	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* CrowMesh;
	UPROPERTY(VisibleAnywhere)
	USphereComponent* SenseSphere;
	UPROPERTY(VisibleAnywhere)
	class UFloatingPawnMovement* Movement;

	// 
	ECrowState CurrentState = ECrowState::Idle;

	//��Ʈ��
	UPROPERTY(EditDefaultsOnly, Category="CrowMovement")
	float PatrolRadius = 600.0f;
	UPROPERTY(EditDefaultsOnly, Category="CrowMovement")
	float OrbitSpeed = 90.0f;
	float CurrentAngle = 0.f;
	FVector PatrolCenter;

	// ���(�߰�)
	UPROPERTY(EditDefaultsOnly, Category="CrowAlert")
	float AlertRadius = 200.f;
	UPROPERTY(EditDefaultsOnly, Category = "CrowAlert")
	float AlertOrbitSpeed = 180.f;	// �߽߰� ������ ȸ��

	AActor* TargetPolice = nullptr;

	FTimerHandle LifeTimer;

	AActor* CrowOwner;

	void DestroyCrow();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;



};
