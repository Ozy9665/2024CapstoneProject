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

void InitializeAISession(const int);
void StartAIWorker();
void StopAIWorker();
void UpdatePoliceAI();
FVector CalculateDir();
void BroadcastPoliceAIState();
void AIWorkerLoop();
