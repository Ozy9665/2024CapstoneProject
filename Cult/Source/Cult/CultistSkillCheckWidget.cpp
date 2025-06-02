// Fill out your copyright notice in the Description page of Project Settings.


#include "CultistSkillCheckWidget.h"
#include "Components/Image.h"

void UCultistSkillCheckWidget::NativeConstruct()
{
    Super::NativeConstruct();
    CursorAngle = 0.f;
    LastRotation = 0.f;
    AccumulatedRotation = 0.f;
    bIsRunning = false;
}

void UCultistSkillCheckWidget::StartSkillCheck(float InRotationSpeed)
{
    RotationSpeed = InRotationSpeed;
    CursorAngle = 0.f;
    bIsRunning = true;

    float BaseAngle = FMath::RandRange(30.f, 330.f);
    float RangeSize = 60.f;

    SuccessAngleMin = BaseAngle;
    SuccessAngleMax = BaseAngle + RangeSize;

    if (Image_SuccessZone)
    {
        FWidgetTransform Transform = Image_SuccessZone->RenderTransform;
        Transform.Angle = BaseAngle;
        Image_SuccessZone->SetRenderTransform(Transform);
    }

    //if (Image_Cursor)
    //{
    //    FWidgetTransform Transform = Image_Cursor->RenderTransform;
    //    Transform.Translation = FVector2D(0.f, -150); 
    //    Image_Cursor->SetRenderTransform(Transform);
    //}

    AccumulatedRotation = 0.f;
    LastRotation = 0.f;
}

void UCultistSkillCheckWidget::StopSkillCheck()
{
    bIsRunning = false;
}

void UCultistSkillCheckWidget::OnInputPressed()
{
    if (!bIsRunning) return;

    // 0~360 ������ ����ȭ
    float NormalizedAngle = FMath::Fmod(CursorAngle, 360.0f);
    if (NormalizedAngle < 0) NormalizedAngle += 360.0f;

    bool bSuccess = (NormalizedAngle >= SuccessAngleMin && NormalizedAngle <= SuccessAngleMax);
    OnSkillCheckResult.Broadcast(bSuccess);
    StopSkillCheck();
    RemoveFromParent();
}

void UCultistSkillCheckWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bIsRunning || !Image_Cursor) return;

    CursorAngle += RotationSpeed * InDeltaTime;
    if (CursorAngle >= 360.f)
        CursorAngle -= 360.f;

    // Ŀ�� ���� ����
    float DeltaRotation = FMath::FindDeltaAngleDegrees(LastRotation, CursorAngle);
    AccumulatedRotation += FMath::Abs(DeltaRotation);
    LastRotation = CursorAngle;

    // ���� ���� �ʰ� �� ���� ���� ó��
    if (AccumulatedRotation >= MaxAllowedRotation)
    {
        OnSkillCheckResult.Broadcast(false); // ���� ����
        StopSkillCheck();
        RemoveFromParent();
        return;
    }
    // Ŀ�� ȸ���� ���� (��ġ ��� ����!)
    FWidgetTransform NewTransform;
    NewTransform.Angle = CursorAngle + 90.f; // ���� ������ ���� ������
    Image_Cursor->SetRenderTransform(NewTransform);
}