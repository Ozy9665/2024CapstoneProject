// Fill out your copyright notice in the Description page of Project Settings.


#include "MyLegacyCameraShake.h"

UMyLegacyCameraShake::UMyLegacyCameraShake()
{
	// 시간
	OscillationDuration = 2.5f;
	OscillationBlendInTime = 0.1f;
	OscillationBlendOutTime = 0.1f;

	// 위치, 회전축 진동

	RotOscillation.Pitch.Amplitude = 3.0f;
	RotOscillation.Pitch.Frequency = 50.0f;

	RotOscillation.Yaw.Amplitude = 3.0f;
	RotOscillation.Yaw.Frequency = 50.0f;

	RotOscillation.Roll.Amplitude = 2.0f;
	RotOscillation.Roll.Frequency = 40.0f;

	LocOscillation.Z.Amplitude = 5.0f; 
	LocOscillation.Z.Frequency = 60.0f; 

	LocOscillation.X.Amplitude = 3.0f; 
	LocOscillation.X.Frequency = 40.0f;

	LocOscillation.Y.Amplitude = 3.0f; 
	LocOscillation.Y.Frequency = 40.0f;
}