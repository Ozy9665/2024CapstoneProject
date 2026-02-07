#pragma once

#include "Protocol.h"
#include "map.h"

void AddCutltistAi(int, uint8_t, int);

void CultistAIWorkerLoop();

template <typename PacketT>
void BroadcastCultistAIState(const SESSION& ai, const PacketT* packet);

void ApplyBatonHitToAI(SESSION&, const Vec3&);

constexpr float RAD_TO_DEG{ 180.f / PI };
constexpr float fixed_dt{ 1.0f / 60.0f };
constexpr float pushDist{ 120.f };
constexpr float REPATH_DIST{ 200.f };
