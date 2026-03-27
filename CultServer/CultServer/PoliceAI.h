#pragma once

#include "Protocol.h"
#include "map.h"

void AddPoliceAi(int, uint8_t, int);
void KillPoliceAi(int ai_id);

void PoliceAIWorkerLoop();

template <typename PacketT>
void BroadcastPoliceAIState(const SESSION& ai, const PacketT* packet);
