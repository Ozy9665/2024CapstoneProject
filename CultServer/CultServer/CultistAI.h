#pragma once

#include "Protocol.h"

class NAVMESH;

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
    NAVMESH* nav;

    explicit CultistAIController(SESSION*);

    // Condition
    bool CanChase();
    bool CanRunaway();
    bool CanHeal();
    bool CanRitual();
    bool CanMove() const;

    // Action
    void Patrol(float);
    void Chase(float);
    void Runaway(float);
    void Heal(float);
    void Ritual(float);

    void Update(float) override;
    void ApplyBatonHit(const Vec3&);

private:
    void UpdateBlackboard(float);
    void RunBehaviorTree(float);
    void UpdateRitualTarget();

    void MoveToNearestTriangle(const Vec3&);
    void StopMovement();
    bool SnapPositionByCurrentTri(NAVMESH&, Vec3&);
    void MoveAlongPath(const Vec3&, float);
    int FindNearbyPolice();
    int FindNearbyCultist();
    std::optional<std::pair<FVector, FRotator>> GetHealMovePoint(int);

};