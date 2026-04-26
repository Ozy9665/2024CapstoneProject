#pragma once

#include "Protocol.h"
#include "map.h"

void AddPoliceAi(int, uint8_t, int);

void KillPoliceAi(int ai_id);

void PoliceAIWorkerLoop();

// Dog AI
// Condition

class HasTargetIdNode : public BTNode {
    bool Run(AIController&, float) override;
};

class NeedFollowOwnerNode : public BTNode {
    bool Run(AIController&, float) override;
};

class ShouldStopChaseNode : public BTNode {
    bool Run(AIController&, float) override;
};

// Action
class DogChaseNode : public BTNode {
    bool Run(AIController&, float) override;
};

class DogStopNode : public BTNode {
    bool Run(AIController&, float) override;
};

class DogFollowNode : public BTNode { 
    bool Run(AIController&, float) override; 
};

class DogExploreNode : public BTNode {
    bool Run(AIController&, float) override; 
};

class DogAIController : public AIController
{
public:
    bool bInitialized = false;

    DogBlackboard db;
    std::unique_ptr<BTNode> root;

    explicit DogAIController(SESSION* o);

    void Init();
    void Update(float);

    void UpdateBlackboard(float);
    void RunBehaviorTree(float);

    bool HasTargetId();
    bool NeedFollowOwner();
    bool ShouldStopChase();

    void Chase(float);
    void Stop(float);
    void Follow(float);
    void Explore(float);
    
private:
    void MoveAlongPathDog(const Vec3&, float);
    int FindNearbyCultistForDog();
};


// Police AI
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

class PoliceAIController : public AIController {
public:
    PoliceBlackboard bb;
    std::unique_ptr<BTNode> root;
    std::unique_ptr<DogAIController> dogAI;

    explicit PoliceAIController(SESSION* o);

    void Update(float) override;
    void UpdateBlackboard(float);
    void RunBehaviorTree(float);

    // Condition
    bool CanBatonAttack();
    bool CanShoot();
    bool CanTaser();
    bool CanPistol();
    bool CanChase();

    // Action
    void BatonAttack(float);
    void TaserShoot(float);
    void PistolShoot(float);
    void Chase(float);
    void Patrol(float);
    void BeginBehaviorLock(float);

    bool HasLineOfSight(int);
private:
    void StopMovement();
    void MoveAlongPath(const Vec3&, float);
    void SnapToNavMesh();
    void MoveToNearestTriangle(const Vec3&);
    int FindNearbyCultist();
};
