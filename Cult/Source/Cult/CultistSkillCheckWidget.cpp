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
    float RangeSize = 100.f;

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

    // 0~360 범위로 정규화
    float NormalizedAngle = FMath::Fmod(CursorAngle+ AngleOffset, 360.0f);
    if (NormalizedAngle < 0) NormalizedAngle += 360.0f;

    bool bSuccess = false;

    if (SuccessAngleMin <= SuccessAngleMax)
    {
        bSuccess = (NormalizedAngle >= SuccessAngleMin && NormalizedAngle <= SuccessAngleMax);
    }
    else
    {
        // 영역이 330도 ~ 30도처럼 범위를 넘는 경우
        bSuccess = (NormalizedAngle >= SuccessAngleMin || NormalizedAngle <= SuccessAngleMax);
    }

    UE_LOG(LogTemp, Warning, TEXT("Angle: %.1f | Success: [%0.1f ~ %0.1f] | Result: %s"), NormalizedAngle, SuccessAngleMin, SuccessAngleMax, bSuccess ? TEXT("TRUE") : TEXT("FALSE"));


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


    float Radians = FMath::DegreesToRadians(CursorAngle);
    FVector2D LocalCenter = MyGeometry.GetLocalSize() * 0.5f;


    float X = Radius * FMath::Cos(Radians);
    float Y = Radius * FMath::Sin(Radians);  

    FWidgetTransform NewTransform;
    NewTransform.Translation = FVector2D(X, Y);  // 중심 기준 상대 좌표
    //NewTransform.Translation = FVector2D(X - LocalCenter.X, -(Y - LocalCenter.Y)); // 중심 기준 좌표로 보정
    NewTransform.Angle = CursorAngle + 90.f;


    Image_Cursor->SetRenderTransform(NewTransform);
}