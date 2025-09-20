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
	SpawnRise,
	Patrol,
	Alert,
	Dive,	// 발견 후 돌진
	Controlled	// 유체이탈
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
	void EndAlertState();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 컴포넌트
	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* CrowMesh;
	UPROPERTY(VisibleAnywhere)
	USphereComponent* SenseSphere;
	UPROPERTY(VisibleAnywhere)
	class UFloatingPawnMovement* Movement;

	// 
	ECrowState CurrentState = ECrowState::Idle;

	//패트롤
	UPROPERTY(EditDefaultsOnly, Category="CrowMovement")
	float PatrolRadius = 600.0f;
	UPROPERTY(EditDefaultsOnly, Category="CrowMovement")
	float OrbitSpeed = 90.0f;
	float CurrentAngle = 0.f;
	FVector PatrolCenter;

	UPROPERTY(EditDefaultsOnly, Category = "CrowMovement")
	float PatrolHeight = 500.f;

	// 탐지
	UPROPERTY(EditAnywhere, Category="CrowSense")
	float SenseRadius = 800.f;
	UPROPERTY(EditAnywhere, Category="CrowSense")
	float SenseInterval = 0.2f;
	//	탐지 
	//      - 내부 타이머
	float SenseTimer = 0.f; 
	void DetectPolice();



	// 경고(발견)
	UPROPERTY(EditDefaultsOnly, Category="CrowAlert")
	float AlertRadius = 200.f;
	UPROPERTY(EditDefaultsOnly, Category = "CrowAlert")
	float AlertOrbitSpeed = 180.f;	// 발견시 빠르게 회전

	float SpawnElapsed = 0.f;

	AActor* TargetPolice = nullptr;


	void DestroyCrow();


	// 타이머 및 시간 파라미터


	UPROPERTY(EditAnywhere, Category="Crow_Timing")
	float PatrolDuration = 12.f;	// 기본 정찰 지속
	UPROPERTY(EditAnywhere, Category = "Crow_Timing")
	float AlertDuration = 5.f;	// 발견 후 지속
	UPROPERTY(EditAnywhere, Category = "Crow_Timing")
	float ControlDuration = 6.f;	// 빙의 지속

	FTimerHandle PatrolTimer;
	FTimerHandle AlertTimer;
	FTimerHandle ControlTimer;

	// 돌진(시야방해) 파라미터
	UPROPERTY(EditAnywhere, Category="Crow_Dive")	// 돌진
	float DiveSpeed = 2200.f;
	UPROPERTY(EditAnywhere, Category = "Crow_Dive")
	float DiveHitRadius = 120.f;
	UPROPERTY(EditAnywhere, Category = "Crow_Effect")	// 돌진 후 처리관련
	float BlindSeconds = 1.8f;
	UPROPERTY(EditAnywhere, Category = "Crow_Dive")
	float StunSeconds = 0.6f;

	// 컨트롤 파라미터
	UPROPERTY(EditAnywhere, Category="Crow_Control")
	float ControlMoveSpeed = 900.f;
	UPROPERTY(EditAnywhere, Category = "Crow_Control")
	float ControlRiseSpeed = 500.f;
	UPROPERTY(EditAnywhere, Category = "Crow_Control")
	float ControlTurnSpeed = 90.f;	// 초당 회전

	// 조종 모드 시 값
	float AxisForward = 0.f, AxisRight = 0.f, AxisUp = 0.f;
	float AxisTurn = 0.f, AxisLookUp = 0.f;

	// 오너 / 컨르롤러 관리
	AActor* CrowOwner = nullptr;
	APlayerController* CachedPC = nullptr;
	APawn* OwnerPawn = nullptr;

	// 종료 관리
	void OnPatrolExpire();
	void OnAlertExpire();
	void OnControlExpire();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;



};
