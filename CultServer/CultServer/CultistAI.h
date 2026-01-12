#pragma once

#include "Protocol.h"
#include "map.h"

void AddCutltistAi(int, uint8_t, int);

void CultistAIWorkerLoop();

template <typename PacketT>
void BroadcastCultistAIState(const SESSION& ai, const PacketT* packet);
