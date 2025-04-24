// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FadeWidget.generated.h"

/**
 * 
 */
UCLASS()
class CULT_API UFadeWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:

	// ���ε��� �ִϸ��̼�
	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* FadeAnim;

	UFUNCTION(BlueprintCallable)
	void PlayFadeAnim();
};
