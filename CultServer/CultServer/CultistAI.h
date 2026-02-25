#pragma once

#include "Protocol.h"
#include "map.h"

void AddCutltistAi(int, uint8_t, int);

void CultistAIWorkerLoop();

template <typename PacketT>
void BroadcastCultistAIState(const SESSION& ai, const PacketT* packet);

void ApplyBatonHitToAI(SESSION&, const Vec3&);

std::optional<std::pair<FVector, FRotator>> GetMovePoint(int, int);

constexpr float RAD_TO_DEG{ 180.f / PI };
constexpr float fixed_dt{ 1.0f / 60.0f };
constexpr float pushDist{ 120.f };
constexpr float REPATH_DIST{ 200.f };
constexpr int sampleCount{ 20 };
constexpr float sampleRadius{ 2000.f };
constexpr float ALTAR_TRIGGER_RANGE{ 600.f };
constexpr float ALTAR_TRIGGER_RANGE_SQ{ ALTAR_TRIGGER_RANGE * ALTAR_TRIGGER_RANGE };
constexpr float CHASE_START_RANGE{ 1500.f };
constexpr float CHASE_STOP_RANGE{ 200.f };