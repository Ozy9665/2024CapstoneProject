#pragma once

#include "Protocol.h"
#include "map.h"

void AddPoliceAi(int, uint8_t, int);
void KillPoliceAi(int ai_id);

void PoliceAIWorkerLoop();

template <typename PacketT>
void BroadcastPoliceAIState(const SESSION& ai, const PacketT* packet);

// ai controller
class BTNode {
public:
    virtual bool Run(AIController& ai, float dt) = 0;
    virtual ~BTNode() = default;
};

class Selector : public BTNode {
public:
    std::vector<std::unique_ptr<BTNode>> children;
    bool Run(AIController& ai, float dt) override;
};

class Sequence : public BTNode {
public:
    std::vector<std::unique_ptr<BTNode>> children;
    bool Run(AIController& ai, float dt) override;
};

// Condition
class CanBatonAttackNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CanShootNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CanTaserNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CanPistolNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class CanChaseNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

// Action
class BatonAttackNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class AimNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class TaserShootNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class PistolShootNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class ChaseNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

class PatrolNode : public BTNode {
public:
    bool Run(AIController& ai, float dt) override;
};

// Police AI
class PoliceAIController : public AIController {
public:
    PoliceBlackboard bb;
    std::unique_ptr<BTNode> root;

    PoliceAIController(SESSION* o);

    void Update(float dt) override;

    void UpdateBlackboard(float dt);
    void RunBehaviorTree(float dt);

    // Condition
    bool CanBatonAttack();
    bool CanShoot();
    bool CanTaser();
    bool CanPistol();
    bool CanChase();

    // Action
    void BatonAttack(float dt);
    void TaserShoot(float dt);
    void PistolShoot(float dt);
    void Chase(float dt);
    void Patrol(float dt);
};