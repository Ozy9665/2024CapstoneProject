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
	Dive,	// �߰� �� ����
	Controlled	// ��ü��Ż
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

	UPROPERTY(EditDefaultsOnly, Category = "CrowMovement")
	float PatrolHeight = 500.f;

	// Ž��
	UPROPERTY(EditAnywhere, Category="CrowSense")
	float SenseRadius = 800.f;
	UPROPERTY(EditAnywhere, Category="CrowSense")
	float SenseInterval = 0.2f;
	//	Ž�� 
	//      - ���� Ÿ�̸�
	float SenseTimer = 0.f; 
	void DetectPolice();



	// ���(�߰�)
	UPROPERTY(EditDefaultsOnly, Category="CrowAlert")
	float AlertRadius = 200.f;
	UPROPERTY(EditDefaultsOnly, Category = "CrowAlert")
	float AlertOrbitSpeed = 180.f;	// �߽߰� ������ ȸ��

	float SpawnElapsed = 0.f;

	AActor* TargetPolice = nullptr;


	void DestroyCrow();


	// Ÿ�̸� �� �ð� �Ķ����


	UPROPERTY(EditAnywhere, Category="Crow_Timing")
	float PatrolDuration = 12.f;	// �⺻ ���� ����
	UPROPERTY(EditAnywhere, Category = "Crow_Timing")
	float AlertDuration = 5.f;	// �߰� �� ����
	UPROPERTY(EditAnywhere, Category = "Crow_Timing")
	float ControlDuration = 6.f;	// ���� ����

	FTimerHandle PatrolTimer;
	FTimerHandle AlertTimer;
	FTimerHandle ControlTimer;

	// ����(�þ߹���) �Ķ����
	UPROPERTY(EditAnywhere, Category="Crow_Dive")	// ����
	float DiveSpeed = 2200.f;
	UPROPERTY(EditAnywhere, Category = "Crow_Dive")
	float DiveHitRadius = 120.f;
	UPROPERTY(EditAnywhere, Category = "Crow_Effect")	// ���� �� ó������
	float BlindSeconds = 1.8f;
	UPROPERTY(EditAnywhere, Category = "Crow_Dive")
	float StunSeconds = 0.6f;

	// ��Ʈ�� �Ķ����
	UPROPERTY(EditAnywhere, Category="Crow_Control")
	float ControlMoveSpeed = 900.f;
	UPROPERTY(EditAnywhere, Category = "Crow_Control")
	float ControlRiseSpeed = 500.f;
	UPROPERTY(EditAnywhere, Category = "Crow_Control")
	float ControlTurnSpeed = 90.f;	// �ʴ� ȸ��

	// ���� ��� �� ��
	float AxisForward = 0.f, AxisRight = 0.f, AxisUp = 0.f;
	float AxisTurn = 0.f, AxisLookUp = 0.f;

	// ���� / �����ѷ� ����
	AActor* CrowOwner = nullptr;
	APlayerController* CachedPC = nullptr;
	APawn* OwnerPawn = nullptr;

	// ���� ����
	void OnPatrolExpire();
	void OnAlertExpire();
	void OnControlExpire();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;



};
