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
	
	// �޽�
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* MeshComp;

	// �ݸ���
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	class UBoxComponent* CollisionComp;

	// ���� �� üũ
	bool bPlayerInRange;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ritual")
	int NumCultistsInRange = 0;



	// �浹ó��
 

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);


	// OvelappedComp => �ݸ��� ������Ʈ => �� �̺�Ʈ�� �߻� => ������ �ݸ����ڽ�
	// OtherActor : �浹�� �ٸ� ���� (cast�� ���Ͽ� Cultist�� ����)
	// OtherComp : ������ Ȱ��x . ������ Cultist���� �浹��
	// bFromSweep : �̵�������
	// SweepResult : �浹����(��ġ,���� ��)

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);


	// Called every frame
	virtual void Tick(float DeltaTime) override;



	// �ǽ�
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ritual")
	float RitualGauge = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ritual")
	float BaseGainRate = 5.0f;


	UFUNCTION(BlueprintCallable, Category="Ritual")
	void IncreaseRitualGauge();


};
