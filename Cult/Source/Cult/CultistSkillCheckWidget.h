// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CultistSkillCheckWidget.generated.h"

/**
 * 
 */
UCLASS()
class CULT_API UCultistSkillCheckWidget : public UUserWidget
{
	GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable)
    void StartSkillCheck(float InRotationSpeed = 180.f);

    UFUNCTION(BlueprintCallable)
    void StopSkillCheck();

    UFUNCTION(BlueprintCallable)
    void OnInputPressed();  // ���ε��� Ű �Է�


    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillCheckResult, bool, bSuccess);

    UPROPERTY(BlueprintAssignable, Category = "SkillCheck")
    FOnSkillCheckResult OnSkillCheckResult;


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck")
    float RotationSpeed = 180.f; // ȸ���ӵ� (��/��)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck")
    float SuccessAngleMin = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck")
    float SuccessAngleMax = 90.0f;
protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    class UImage* Image_Cursor;

private:
    bool bIsRunning = false;
    float CursorAngle = 0.f; // ���� ȸ����
    
    // �� ���� ȸ�� ���� (360�� = 1����)
    float AccumulatedRotation = 0.0f;


    // ������ �������� ȸ����
    float LastRotation = 0.0f;

    // �ִ� ��� ȸ�� ���� ��
    float MaxAllowedRotation = 720.0f; // 2����

    FTimerHandle SkillCheckFailTimerHandle;
};
