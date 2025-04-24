#pragma once

#include <array>
#include <thread>
#include <cmath>
#include "Protocol.h"

constexpr float 파이{ 3.141592 };

struct FVector {
	float x, y, z;
};

struct FRotator {
	float pitch;
	float yaw;
	float roll;

};

std::array<FVector, 4> PatrolPoints	// 임시
{
	FVector{110.f, -1100.f, 2770.f},
	FVector{160.f, -1050.f, 2770.f},
	FVector{120.f, -1000.f, 2770.f},
	FVector{ 90.f, -1070.f, 2770.f}
};
int CurrentPatrolIndex = 0;
int TargetID{ -1 };

void StartAIWorker();
void UpdatePoliceAI();
FVector CalculateDir();
void AIWorkerLoop();