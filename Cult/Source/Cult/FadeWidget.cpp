// Fill out your copyright notice in the Description page of Project Settings.


#include "FadeWidget.h"

void UFadeWidget::PlayFadeAnim()
{
	if (FadeAnim)
	{
		PlayAnimation(FadeAnim);
	}
}