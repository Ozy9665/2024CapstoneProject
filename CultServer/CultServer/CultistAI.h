#pragma once

#include "Protocol.h"

void AddCutltistAi(int, uint8_t, int);
void KillCultistAi(int ai_id);

void CultistAIWorkerLoop();

template <typename PacketT>
void BroadcastCultistAIState(const SESSION& ai, const PacketT* packet);

void ApplyBatonHitToAI(SESSION&, const Vec3&);

std::optional<std::pair<FVector, FRotator>> GetMovePoint(int, int);

struct RunawayCandidate
{
    Vec3 pos;
    float score;
    int tri;

    bool operator<(const RunawayCandidate& other) const
    {
        return score > other.score;
    }
};

class CultistAIController : public AIController {
public:
    CultistBlackboard bb;

    CultistAIController(SESSION* o) : AIController(o) {}

    void Update(float dt) override;
};