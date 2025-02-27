// Fill out your copyright notice in the Description page of Project Settings.


#include "MyLegacyCameraShake.h"

UMyLegacyCameraShake::UMyLegacyCameraShake()
{
	OscillationDuration = 2.5f;
	OscillationBlendInTime = 0.1f;
	OscillationBlendOutTime = 0.1f;

	RotOscillation.Pitch.Amplitude = 5.0f;
	RotOscillation.Pitch.Frequency = 50.0f;

	RotOscillation.Yaw.Amplitude = 5.0f;
	RotOscillation.Yaw.Frequency = 50.0f;
}