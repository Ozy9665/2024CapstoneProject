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
    void OnInputPressed();  // 바인딩할 키 입력


    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillCheckResult, bool, bSuccess);

    UPROPERTY(BlueprintAssignable)
    FOnSkillCheckResult OnSkillCheckResult;


protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    class UImage* Image_Cursor;

private:
    bool bIsRunning = false;
    float CursorAngle = 0.f; // 현재 회전각
    float RotationSpeed = 180.f; // 회전속도 (도/초)

    // 성공 각도 범위
    float SuccessAngleMin = 30.f; // 예시: 우측 상단 기준
    float SuccessAngleMax = 60.f;
};
