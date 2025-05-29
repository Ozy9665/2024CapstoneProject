#pragma once

#include <array>
#include <thread>
#include <cmath>
#include "Protocol.h"

constexpr float ÆÄÀÌ{ 3.14 };

void InitializeAISession(const int);
void StartAIWorker();
void StopAIWorker();
void UpdatePoliceAI();
FVector CalculateDir();
void BroadcastPoliceAIState();
void AIWorkerLoop();
