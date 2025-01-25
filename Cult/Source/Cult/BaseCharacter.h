// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseCharacter.generated.h"


UENUM(BlueprintType)
enum class ECharacterState : uint8
{
	// ����. ���Ƿ�. ��������
	Normal UMETA(DisplayName="Normal"),
	Stunned UMETA(DisplayName="Stunned"),	// ���� �ŵ� �� ��
	Captured UMETA(DisplayName="Captured"), // �ŵ��� Ư��?
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
	// ====================== ���� ����======================
	// 
	// ü��
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Stats")
	float Health;
	// �̵��ӵ�
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Stats")
	float MovementSpeed;
	// ĳ���� ����
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Character Stats")
	ECharacterState CharacterState;
	
	// ====================== �Լ� ����======================
	//
	// �̵� �Լ�
	void MoveForward(float Value);
	void MoveRight(float Value);
	// ���� �Լ�
	virtual void Attack();
	// ���� ó�� �Լ�
	virtual void TakeDamage(float DamageAmount);
	// ���� ���� �Լ�
	void SetCharacterState(ECharacterState NewState);
	// ü�� Ȯ�� �Լ�
	bool IsAlive() const;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	/*
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	*/
protected:
	// �Է� ����
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
