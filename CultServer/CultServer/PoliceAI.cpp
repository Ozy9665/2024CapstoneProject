#define NOMINMAX

#include "PoliceAI.h"
#include "map.h"
#include <thread>
#include <array>
#include <cmath>
#include <concurrent_priority_queue.h>
#include <concurrent_unordered_set.h>
#include <concurrent_unordered_map.h>
#include <mutex>
#include <queue>

using namespace std;
extern concurrency::concurrent_unordered_map<int, std::shared_ptr<SESSION>> g_users;
extern concurrency::concurrent_unordered_set<int> g_police_ai_ids;
extern std::array<std::pair<Room, MAPTYPE>, MAX_ROOM> g_rooms;
extern MAP NewmapLandmassMap;
extern NAVMESH NewmapLandmassNavMesh;
extern concurrency::concurrent_priority_queue<TIMER_EVENT> timer_queue;
extern std::array<std::array<Altar, ALTAR_PER_ROOM>, MAX_ROOM> g_altars;
extern std::mutex g_room_mtx;
extern std::queue<RoomTask> g_room_q;
extern std::condition_variable g_room_cv;
extern std::vector<int> free_session_ids;
extern std::mutex free_id_mtx;

void AddPoliceAi(int ai_id, uint8_t ai_role, int room_id)
{
    auto session = std::make_shared<SESSION>(ai_id, ai_role, room_id);
    auto it = g_users.find(ai_id);
    if (it != g_users.end())
    {
        it->second = session;
    }
    else
    {
        g_users.insert({ ai_id, session });
    }

    auto [it_ai, inserted] = g_police_ai_ids.insert(ai_id);

    auto& room = g_rooms[room_id];
    for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i)
    {
        if (room.first.player_ids[i] == -1)
        {
            room.first.player_ids[i] = ai_id;
            room.first.police += 1;
            break;
        }
    }

    std::cout << "[Command] AI added. ID=" << ai_id
        << " role=" << static_cast<int>(ai_role)
        << " room=" << room_id << "\n";
    IdRolePacket pkt{};
    pkt.header = connectionHeader;
    pkt.size = sizeof(IdRolePacket);
    pkt.id = ai_id;
    pkt.role = ai_role;

    BroadcastPoliceAIState(*session, &pkt);
}

void KillPoliceAi(int ai_id)
{
    auto it = g_users.find(ai_id);
    if (it == g_users.end())
        return;

    auto session = it->second;

    if (session->role != 101)
        return;

    IdOnlyPacket pkt;
    pkt.header = DisconnectionHeader;
    pkt.size = sizeof(IdOnlyPacket);
    pkt.id = ai_id;
    BroadcastPoliceAIState(*session, &pkt);

    {
        std::lock_guard<std::mutex> lk(g_room_mtx);
        g_room_q.push(RoomTask{
            ai_id,
            RM_DISCONNECT,
            session->role,
            session->room_id
            });
        g_room_cv.notify_one();
    }

    // AI 목록에서 제거
    session->resetForReuse();
    {
        std::lock_guard<std::mutex> lk(free_id_mtx);
        free_session_ids.push_back(ai_id);
    }
    std::cout << "[Command] AI removed. ID=" << ai_id << "\n";
}

void PoliceAIWorkerLoop()
{
    using clock = std::chrono::steady_clock;
    auto nextTick = clock::now();

    while (true)
    {
        auto now = clock::now();

        std::chrono::duration<float> delta = now - nextTick + std::chrono::duration<float>(fixed_dt);
        float dt = delta.count();

        if (dt < 0.f) dt = 0.f;
        if (dt > 0.05f) dt = 0.05f;

        for (int ai_id : g_police_ai_ids)
        {
            auto it = g_users.find(ai_id);
            if (it == g_users.end())
                continue;

            auto session = it->second;
            if (session->role != 101)
                continue;

            auto aiPtr = session->ai;
            if (!aiPtr)
                continue;

            auto* policeAI = dynamic_cast<PoliceAIController*>(aiPtr.get());
            if (!policeAI)
                continue;

            if (policeAI->bb.ai_state == AIState::Free)
                continue;

            auto& st = session->police_state;

            session->ai->Update(dt);
            
            PolicePacket packet{};
            packet.header = policeHeader;
            packet.size = sizeof(PolicePacket);
            packet.state = session->police_state;
            BroadcastPoliceAIState(*session, &packet);
        }

        nextTick += std::chrono::duration_cast<clock::duration>(
            std::chrono::duration<float>(fixed_dt));

        std::this_thread::sleep_until(nextTick);
    }
}

template <typename PacketT>
void BroadcastPoliceAIState(const SESSION& session, const PacketT* packet)
{
    if (session.role != 101) // Police AI만
        return;

    int room_id = session.room_id;
    if (room_id < 0 || room_id >= MAX_ROOM)
    {
        return;
    }

    auto& room = g_rooms[room_id].first;

    for (int pid : room.player_ids)
    {
        if (pid == -1 || pid == session.id)
            continue;

        auto it = g_users.find(pid);
        if (it == g_users.end())
            continue;

        auto target = it->second;

        if (!target->isValidSocket() || target->state == ST_FREE)
            continue;

        target->do_send_packet(reinterpret_cast<void*>(const_cast<PacketT*>(packet)));
    }
}

// Movements
static float Dist(const Vec3& a, const Vec3& b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static void StopMovement(SESSION& session)
{
    session.police_state.VelocityX = 0.f;
    session.police_state.VelocityY = 0.f;
    session.police_state.VelocityZ = 0.f;
    session.police_state.Speed = 0.f;
}

static void MoveAlongPath(SESSION& session, const Vec3& targetPos, float deltaTime)
{
    if (!session.ai)
        return;

    Vec3 cur{
        session.cultist_state.PositionX,
        session.cultist_state.PositionY,
        session.cultist_state.PositionZ
    };

    if (Dist(cur, targetPos) <= ARRIVE_RANGE)
    {
        StopMovement(session);
        return;
    }

    auto* policeAI = dynamic_cast<PoliceAIController*>(session.ai.get());
    if (!policeAI)
        return;

    float dx = targetPos.x - policeAI->bb.lastTargetPos.x;
    float dy = targetPos.y - policeAI->bb.lastTargetPos.y;
    float dist2 = dx * dx + dy * dy;

    if (dist2 > REPATH_DIST * REPATH_DIST)
    {
        // 타겟이 충분히 이동, 경로 무효화
        policeAI->bb.lastTargetPos = targetPos;
        policeAI->bb.path.clear();
    }

    if (policeAI->bb.path.empty())
    {
        std::vector<int> triPath;
        if (!NewmapLandmassNavMesh.FindTriPath(cur, targetPos, triPath))
        {
            StopMovement(session);
            policeAI->bb.path.clear();
            return;
        }

        std::vector<std::pair<Vec3, Vec3>> portals;
        NewmapLandmassNavMesh.BuildPortals(triPath, portals);

        if (portals.empty()) {
            StopMovement(session);
            return;
        }

        std::vector<Vec3> smoothPath;
        if (!NewmapLandmassNavMesh.SmoothPath(cur, targetPos, portals, smoothPath) ||
            smoothPath.size() < 2)
        {
            StopMovement(session);
            return;
        }
        policeAI->bb.path = smoothPath;
    }

    if (policeAI->bb.path.empty())
    {
        StopMovement(session);
        return;
    }

    if (policeAI->bb.path.size() < 1)
    {
        std::cout << "[FATAL] path empty before next selection\n";
        StopMovement(session);
        return;
    }

    // 다음 목표 노드
    Vec3 next;
    if (policeAI->bb.path.size() >= 2) {
        next = policeAI->bb.path[1];
    }
    else {
        next = policeAI->bb.path[0];
    }

    Vec3 dir{
        next.x - cur.x,
        next.y - cur.y,
        0.f
        // next.z - cur.z
    };
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    // float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    const float speed = 300.f; // 600cm/s
    if (len <= speed * deltaTime)
    {
        policeAI->bb.path.erase(policeAI->bb.path.begin());

        if (policeAI->bb.path.empty())
        {
            StopMovement(session);
            return;
        }

        next = policeAI->bb.path[0];

        dir.x = next.x - cur.x;
        dir.y = next.y - cur.y;
        len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        std::cout << " if (len <= speed * deltaTime)" << std::endl;
    }

    if (len < 1e-3f)
    {
        StopMovement(session);
        return;
    }

    dir.x /= len;
    dir.y /= len;
    // dir.z /= len;

    // 위치 갱신
    session.cultist_state.PositionX += dir.x * speed * deltaTime;
    session.cultist_state.PositionY += dir.y * speed * deltaTime;
    // ai.cultist_state.PositionZ += dir.z * speed * deltaTime;

    session.cultist_state.VelocityX = dir.x * speed;
    session.cultist_state.VelocityY = dir.y * speed;
    session.cultist_state.VelocityZ = 0.f;
    session.cultist_state.Speed = std::sqrt(
        session.cultist_state.VelocityX * session.cultist_state.VelocityX +
        session.cultist_state.VelocityY * session.cultist_state.VelocityY
    );

    // state 회전 갱신
    if (len > 1e-3f)
    {
        session.cultist_state.RotationYaw =
            std::atan2(dir.y, dir.x) * RAD_TO_DEG;
    }
}

// PoliceAIController
PoliceAIController::PoliceAIController(SESSION* o)
    : AIController(o)
{
    auto rootSelector = std::make_unique<Selector>();

    // Baton
    {
        auto seq = std::make_unique<Sequence>();
        seq->children.push_back(std::make_unique<CanBatonAttackNode>());
        seq->children.push_back(std::make_unique<BatonAttackNode>());
        rootSelector->children.push_back(std::move(seq));
    }

    // Shoot
    {
        auto seq = std::make_unique<Sequence>();
        seq->children.push_back(std::make_unique<CanShootNode>());
        seq->children.push_back(std::make_unique<AimNode>());
        auto weaponSelector = std::make_unique<Selector>();
           
        // Taser
        {
            auto taserSeq = std::make_unique<Sequence>();
            taserSeq->children.push_back(std::make_unique<CanTaserNode>());
            taserSeq->children.push_back(std::make_unique<TaserShootNode>());
            weaponSelector->children.push_back(std::move(taserSeq));
        }

        // Pistol
        {
            auto pistolSeq = std::make_unique<Sequence>();
            pistolSeq->children.push_back(std::make_unique<CanPistolNode>());
            pistolSeq->children.push_back(std::make_unique<PistolShootNode>());
            weaponSelector->children.push_back(std::move(pistolSeq));
        }

        seq->children.push_back(std::move(weaponSelector));
        rootSelector->children.push_back(std::move(seq));
    }

    // Chase
    {
        auto seq = std::make_unique<Sequence>();
        seq->children.push_back(std::make_unique<CanChaseNode>());
        seq->children.push_back(std::make_unique<ChaseNode>());
        rootSelector->children.push_back(std::move(seq));
    }

    // Patrol (fallback)
    rootSelector->children.push_back(std::make_unique<PatrolNode>());

    root = std::move(rootSelector);
}

bool Selector::Run(AIController& ai, float dt)
{
    for (auto& c : children)
    {
        if (c->Run(ai, dt))
            return true;
    }
    return false;
}

bool Sequence::Run(AIController& ai, float dt)
{
    for (auto& c : children)
    {
        if (!c->Run(ai, dt))
            return false;
    }
    return true;
}

// Condition Node
bool CanChaseNode::Run(AIController& ai, float)
{
    return static_cast<PoliceAIController&>(ai).CanChase();
}

bool CanBatonAttackNode::Run(AIController& ai, float)
{
    return static_cast<PoliceAIController&>(ai).CanBatonAttack();
}

bool CanShootNode::Run(AIController& ai, float)
{
    return static_cast<PoliceAIController&>(ai).CanShoot();
}

bool CanTaserNode::Run(AIController& ai, float)
{
    return static_cast<PoliceAIController&>(ai).CanTaser();
}

bool CanPistolNode::Run(AIController& ai, float)
{
   return static_cast<PoliceAIController&>(ai).CanPistol();
}


// Action Node
bool PatrolNode::Run(AIController& ai, float dt)
{
    static_cast<PoliceAIController&>(ai).Patrol(dt);
    return true;
}

bool ChaseNode::Run(AIController& ai, float dt)
{
    static_cast<PoliceAIController&>(ai).Chase(dt);
    return true;
}

bool BatonAttackNode::Run(AIController& ai, float dt)
{
    static_cast<PoliceAIController&>(ai).BatonAttack(dt);
    return true;
}

bool AimNode::Run(AIController& ai, float dt)
{
    auto& ctrl = static_cast<PoliceAIController&>(ai);

    if (ctrl.bb.aim_target == -1)
    {
        ctrl.bb.aim_target = ctrl.bb.target_id;
        ctrl.bb.aim_time = 0.f;
    }

    //if (!HasLineOfSight(ctrl.owner, ctrl.bb.aim_target))
    //{
    //    ctrl.bb.aim_target = -1;
    //    ctrl.bb.aim_time = 0.f;
    //    ctrl.owner->police_state.bIsAiming = false;
    //    return false;
    //}

    ctrl.bb.aim_time += dt;
    ctrl.owner->police_state.bIsAiming = true;

    if (ctrl.bb.aim_time < 1.0f)
        return false;

    return true;
}

bool TaserShootNode::Run(AIController& ai, float dt)
{
    auto& ctrl = static_cast<PoliceAIController&>(ai);

    ctrl.TaserShoot(dt);

    ctrl.bb.aim_time = 0.f;
    ctrl.owner->police_state.bIsAiming = false;

    return true;
}

bool PistolShootNode::Run(AIController& ai, float dt)
{
    auto& ctrl = static_cast<PoliceAIController&>(ai);

    ctrl.PistolShoot(dt);

    ctrl.bb.aim_time = 0.f;
    ctrl.owner->police_state.bIsAiming = false;

    return true;
}


// Condition
bool PoliceAIController::CanChase()
{
    return bb.target_id != -1;
}

bool PoliceAIController::CanBatonAttack()
{
    return bb.target_id != -1 &&
        bb.last_dist_to_target < BATON_RANGE;
}

bool PoliceAIController::CanShoot()
{
    if (bb.aim_target != -1)
        return true;

    // line trace 추가
    return bb.last_dist_to_target < TASER_RANGE;
}

bool PoliceAIController::CanTaser()
{
    // line trace 추가
    return bb.last_dist_to_target < TASER_RANGE;
}

bool PoliceAIController::CanPistol()
{
    // line trace 추가
    return bb.last_dist_to_target < PISTOL_RANGE;
}

// Action
void PoliceAIController::Patrol(float dt)
{
    Vec3 cur{
        owner->police_state.PositionX,
        owner->police_state.PositionY,
        owner->police_state.PositionZ
    };

    // 목표 없으면 생성
    if (!bb.has_patrol_target)
    {
        int curTri = NewmapLandmassNavMesh.FindContainingTriangle(cur);
        if (curTri < 0)
            return;

        int randomTri = NewmapLandmassNavMesh.GetRandomTriangle(curTri, 10);
        if (randomTri < 0)
            return;

        bb.patrol_target = NewmapLandmassNavMesh.GetTriCenter(randomTri);
        bb.has_patrol_target = true;

        bb.stuck_ticks = 0;
        bb.last_dist_to_target = FLT_MAX;
        bb.path.clear();
    }

    float dist = Dist(cur, bb.patrol_target);

    // 도착
    if (dist < CHASE_STOP_RANGE)
    {
        bb.has_patrol_target = false;
        bb.path.clear();
        return;
    }

    // stuck 체크
    if (dist > bb.last_dist_to_target - STUCK_RANGE)
    {
        bb.stuck_ticks++;
    }
    else 
    {
        bb.stuck_ticks = 0;
    }
    bb.last_dist_to_target = dist;

    if (bb.stuck_ticks > 60)
    {
        bb.has_patrol_target = false;
        bb.path.clear();
        return;
    }

    MoveAlongPath(*owner, bb.patrol_target, dt);
}

void PoliceAIController::Chase(float dt)
{
    if (bb.target_id < 0)
    {
        bb.path.clear();
        StopMovement(*owner);
        return;
    }

    auto it = g_users.find(bb.target_id);
    if (it == g_users.end())
    {
        bb.target_id = -1;
        bb.path.clear();
        return;
    }

    auto target = it->second;

    Vec3 selfPos{
        owner->police_state.PositionX,
        owner->police_state.PositionY,
        owner->police_state.PositionZ
    };

    Vec3 targetPos{
        target->cultist_state.PositionX,
        target->cultist_state.PositionY,
        target->cultist_state.PositionZ
    };

    float dist = Dist(selfPos, targetPos);

    if (dist <= CHASE_STOP_RANGE)
    {
        bb.path.clear();
        StopMovement(*owner);
        return;
    }

    MoveAlongPath(*owner, targetPos, dt);
}

void PoliceAIController::BatonAttack(float dt)
{
    // 공격 함수 추가
    owner->police_state.bIsAttacking = true;
    StopMovement(*owner);
}

void PoliceAIController::TaserShoot(float dt)
{
    // 공격 함수 추가
    owner->police_state.bIsShooting = true;
}

void PoliceAIController::PistolShoot(float dt)
{
    // 공격 함수 추가
    owner->police_state.bIsShooting = true;
}

// BT
void PoliceAIController::UpdateBlackboard(float dt)
{
    // BB업데이트
}

void PoliceAIController::RunBehaviorTree(float dt)
{
    if (root)
        root->Run(*this, dt);
}

void PoliceAIController::Update(float dt)
{
    UpdateBlackboard(dt);
    RunBehaviorTree(dt);
}