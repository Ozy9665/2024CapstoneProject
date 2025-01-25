// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseCharacter.generated.h"


UENUM(BlueprintType)
enum class ECharacterState : uint8
{
	// 상태. 임의로. 수정가능
	Normal UMETA(DisplayName="Normal"),
	Stunned UMETA(DisplayName="Stunned"),	// 경찰 신도 둘 다
	Captured UMETA(DisplayName="Captured"), // 신도만 특정?
	Dead UMETA(DisplayName="Dead")
};

UCLASS()
class CULT_API ABaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// ====================== 변수 선언======================
	// 
	// 체력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Stats")
	float Health;
	// 이동속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Stats")
	float MovementSpeed;
	// 캐릭터 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Character Stats")
	ECharacterState CharacterState;
	
	// ====================== 함수 선언======================
	//
	// 이동 함수
	void MoveForward(float Value);
	void MoveRight(float Value);
	// 공격 함수
	virtual void Attack();
	// 피해 처리 함수
	virtual void TakeDamage(float DamageAmount);
	// 상태 변경 함수
	void SetCharacterState(ECharacterState NewState);
	// 체력 확인 함수
	bool IsAlive() const;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	/*
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	*/
protected:
	// 입력 설정
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
