// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CultGameMode.h"
#include "GameFramework/Actor.h"
#include "Altar.generated.h"


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
	// Property
	
	// 메쉬
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* MeshComp;

	// 콜리전
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	class UBoxComponent* CollisionComp;

	// 영역 안 체크
	bool bPlayerInRange;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ritual")
	int NumCultistsInRange = 0;



	// 충돌처리
 

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);


	// OvelappedComp => 콜리전 컴포넌트 => 이 이벤트를 발생 => 제단의 콜리전박스
	// OtherActor : 충돌한 다른 액터 (cast를 통하여 Cultist만 감지)
	// OtherComp : 아직은 활용x . 지금은 Cultist와의 충돌만
	// bFromSweep : 이동중인지
	// SweepResult : 충돌정보(위치,벡터 등)

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);


	// Called every frame
	virtual void Tick(float DeltaTime) override;



	// 의식
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ritual")
	float RitualGauge = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ritual")
	float BaseGainRate = 5.0f;


	UFUNCTION(BlueprintCallable, Category="Ritual")
	void IncreaseRitualGauge();


};
