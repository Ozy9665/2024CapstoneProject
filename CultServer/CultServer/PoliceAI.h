#pragma once

#include <array>
#include <thread>
#include <cmath>
#include "Protocol.h"

constexpr float ÆÄÀÌ{ 3.14 };

struct FVector {
	float x, y, z;
};

struct FRotator {
	float pitch;
	float yaw;
	float roll;

};

void InitializeAISession();
void StartAIWorker();
void UpdatePoliceAI();
FVector CalculateDir();
void AIWorkerLoop();
