// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CultGameMode.h"
#include "GameFramework/Actor.h"
#include "Altar.generated.h"

class ACultistCharacter;
class UNiagaraComponent;


UCLASS()
class CULT_API AAltar : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAltar();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	
	// Property
	
	// 메쉬
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* MeshComp;

	// 콜리전
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	class UBoxComponent* CollisionComp;

	// qte 및 vfx
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UNiagaraComponent* QTEParticleComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	float CurrentGlow = 0;

	// 제단 머터리얼
	UPROPERTY()
	class UMaterialInstanceDynamic* AltarMID;

	// 영역 안 체크
	bool bPlayerInRange;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ritual")
	int NumCultistsInRange = 0;

	// 기본 게이지 증가 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ritual")
	float SlowAutoChargeRate = 0.5f;

	// qte 로직 변수들
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ritual | QTE")
	bool bIsQTEActive = false;	// qte 활성화 여부

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ritual | QTE")
	float QTECurrentAngle = 0.0f;	// 0~360 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual | QTE")
	float QTERotationSpeed = 90.0f;	// 초당 회전 각도

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual | QTE")
	float QTESuccessZoneStartAngle = 330.0f; // 성공영역 시작 각도

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual | QTE")
	float QTESuccessZoneEndAngle = 359.0f;	// 성공영역 끝 각도

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual | QTE")
	float QTEBonus = 10.0f;	// 성공 시 게이지

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual | QTE")
	float QTEPenalty = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ritual | QTE")
	float QTEInterval = 5.0f;	// 다음 QTE까지의 시간

	FTimerHandle QTETriggerTimerHandle;

	UPROPERTY()
	ACultistCharacter* CurrentPerformingCultist;

	// 충돌처리
	// OvelappedComp => 콜리전 컴포넌트 => 이 이벤트를 발생 => 제단의 콜리전박스
	// OtherActor : 충돌한 다른 액터 (cast를 통하여 Cultist만 감지)
	// OtherComp : 아직은 활용x . 지금은 Cultist와의 충돌만
	// bFromSweep : 이동중인지
	// SweepResult : 충돌정보(위치,벡터 등)

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 의식
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ritual")
	float RitualGauge = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "Ritual")
	void AddToRitualGauge(float Amount);

	void CheckRitualComplete();





	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ritual")
	float BaseGainRate = 35.0f;


	UFUNCTION(BlueprintCallable, Category="Ritual")
	void IncreaseRitualGauge();

	FTimerHandle DisableTimerHandle;

	// QTE 함수
	UFUNCTION()
	void StartRitualQTE(ACultistCharacter* PerformingCultist);

	UFUNCTION()
	void StopRitualQTE(ACultistCharacter* PerformingCultist);

	UFUNCTION()
	void OnPlayerInput();

	UFUNCTION()
	void TriggerNextQTE();

	// 제단 id - 추후 레벨에서 지정 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ritual")
	int32 AltarID = 0;

	

	//UPROPERTY(EditDefaultsOnly, Category="Effects")
	//TSubclassOf<class UMatineeCameraShake> EarthquakeEffect;
};
