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
class CultistCanRunawayNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CultistCanChaseNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CultistCanHealNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CultistCanRitualNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

// Action
class CultistRunawayNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CultistChaseNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CultistHealNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CultistRitualNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CultistPatrolNode : public BTNode {
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