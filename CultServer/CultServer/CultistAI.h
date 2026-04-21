#pragma once

#include "Protocol.h"

void AddCutltistAi(int, uint8_t, int);

void KillCultistAi(int ai_id);

void CultistAIWorkerLoop();

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

// Condition
class CanRunawayNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CanChaseNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CanHealNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CanRitualNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

// Action
class RunawayNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class ChaseNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class HealNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class RitualNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class PatrolNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CultistAIController : public AIController {
public:
    CultistBlackboard bb;
    std::unique_ptr<BTNode> root;

    explicit CultistAIController(SESSION*);

    void Update(float) override;

    void UpdateBlackboard(float);
    void RunBehaviorTree(float);

    // Condition
    bool CanChase();
    bool CanRunaway();
    bool CanHeal();
    bool CanRitual();

    // Action
    void Patrol(float);
    void Chase(float);
    void Runaway(float);
    void Heal(float);
    void Ritual(float);

    bool CanMove() const;
    void ApplyBatonHit(const Vec3&);
};