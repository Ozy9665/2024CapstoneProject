// Fill out your copyright notice in the Description page of Project Settings.


#include "CultistSkillCheckWidget.h"

#include "CultistSkillCheckWidget.h"
#include "Components/Image.h"

void UCultistSkillCheckWidget::NativeConstruct()
{
    Super::NativeConstruct();
    CursorAngle = 0.f;
}

void UCultistSkillCheckWidget::StartSkillCheck(float InRotationSpeed)
{
    RotationSpeed = InRotationSpeed;
    CursorAngle = 0.f;
    bIsRunning = true;
}

void UCultistSkillCheckWidget::StopSkillCheck()
{
    bIsRunning = false;
}

void UCultistSkillCheckWidget::OnInputPressed()
{
    if (!bIsRunning) return;

    // 0~360 범위로 정규화
    float NormalizedAngle = FMath::Fmod(CursorAngle, 360.0f);
    if (NormalizedAngle < 0) NormalizedAngle += 360.0f;

    bool bSuccess = (NormalizedAngle >= SuccessAngleMin && NormalizedAngle <= SuccessAngleMax);

    OnSkillCheckResult.Broadcast(bSuccess);
    StopSkillCheck();
}

void UCultistSkillCheckWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bIsRunning || !Image_Cursor) return;

    CursorAngle += RotationSpeed * InDeltaTime;
    if (CursorAngle >= 360.f)
        CursorAngle -= 360.f;

    // 커서 회전 적용
    FWidgetTransform NewTransform = Image_Cursor->RenderTransform;
    NewTransform.Angle = CursorAngle;
    Image_Cursor->SetRenderTransform(NewTransform);
}