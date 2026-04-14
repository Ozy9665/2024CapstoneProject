#define NOMINMAX

#include "PoliceAI.h"
#include <thread>
#include <array>
#include <cmath>
#include <concurrent_priority_queue.h>
#include <concurrent_unordered_set.h>
#include <concurrent_unordered_map.h>
#include <mutex>
#include <queue>
#include "map.h"
#include "MapManager.h"
#include "Combat.h"

using namespace std;
extern concurrency::concurrent_unordered_map<int, std::shared_ptr<SESSION>> g_users;
extern concurrency::concurrent_unordered_set<int> g_police_ai_ids;
extern std::array<std::pair<Room, MAPTYPE>, MAX_ROOM> g_rooms;
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

    broadcast_in_room(*session, &pkt, VIEW_RANGE);
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
    broadcast_in_room(*session, &pkt, VIEW_RANGE);

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

    std::cout << "[Command] AI removed. ID=" << ai_id << "\n";
}

static void StopMovement(SESSION& session)
{
    session.police_state.VelocityX = 0.f;
    session.police_state.VelocityY = 0.f;
    session.police_state.VelocityZ = 0.f;
    session.police_state.Speed = 0.f;
}

static void MoveToNearestTriangle(SESSION& session, const Vec3& cur)
{
    auto* policeAI = dynamic_cast<PoliceAIController*>(session.ai.get());
    if (!policeAI)
        return;

    NAVMESH* nav = GetNavMesh(session.room_id);
    if (nav)
    {
        int tri = nav->FindContainingTriangle(policeAI->bb.lastSnapPos);

        if (tri >= 0)
        {
            Vec3 safe = nav->GetTriCenter(tri);

            session.police_state.PositionX = safe.x;
            session.police_state.PositionY = safe.y;
            session.police_state.PositionZ = safe.z;
        }
        else
        {
            // fallback
            session.police_state.PositionX = 0.f;
            session.police_state.PositionY = 0.f;
        }
    }
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
            broadcast_in_room(*session, &packet, VIEW_RANGE);
            std::cout << "[SEND Police] ID=" << session->id
                << " Pos=("
                << session->police_state.PositionX << ", "
                << session->police_state.PositionY << ", "
                << session->police_state.PositionZ << ")\n";

            DogPacket d{};
            d.header = dogHeader;
            d.size = sizeof(DogPacket);
            d.dog = session->dog;
            broadcast_in_room(*session, &d, VIEW_RANGE);
        }

        nextTick += std::chrono::duration_cast<clock::duration>(
            std::chrono::duration<float>(fixed_dt));

        std::this_thread::sleep_until(nextTick);
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

static void SnapToNavMesh(SESSION& session)
{
    NAVMESH* nav = GetNavMesh(session.room_id);
    if (!nav)
        return;

    Vec3 pos{
        session.police_state.PositionX,
        session.police_state.PositionY,
        session.police_state.PositionZ
    };

    if (!nav->SnapPositionToNavMesh(pos))
        return;

    session.police_state.PositionZ = pos.z;
}

static void MoveAlongPath(SESSION& session, const Vec3& targetPos, float deltaTime)
{
    if (!session.ai)
        return;

    Vec3 cur{
    session.police_state.PositionX,
    session.police_state.PositionY,
    session.police_state.PositionZ
    };

    if (!std::isfinite(cur.x) || !std::isfinite(cur.y))
    {
        std::cout << "[FIX] NaN detected -> reset path\n";

        auto* policeAI = dynamic_cast<PoliceAIController*>(session.ai.get());
        if (!policeAI)
            return;

        policeAI->bb.path.clear();
        policeAI->bb.has_patrol_target = false;

        session.police_state.PositionX = policeAI->bb.lastSnapPos.x;
        session.police_state.PositionY = policeAI->bb.lastSnapPos.y;

        Vec3 safe{
            session.police_state.PositionX,
            session.police_state.PositionY,
            session.police_state.PositionZ
        };

        MoveToNearestTriangle(session, safe);

        return;
    }

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
    float dz = targetPos.z - policeAI->bb.lastTargetPos.z;
    float dist2 = dx * dx + dy * dy + dz * dz;

    if (dist2 > REPATH_DIST * REPATH_DIST)
    {
        // 타겟이 충분히 이동, 경로 무효화
        policeAI->bb.lastTargetPos = targetPos;
        policeAI->bb.path.clear();
    }

    if (policeAI->bb.path.empty())
    {
        NAVMESH* nav = GetNavMesh(session.room_id);
        if (!nav)
            return;

        std::vector<int> triPath;
        if (!nav->FindTriPath(cur, targetPos, triPath))
        {
            StopMovement(session);
            policeAI->bb.path.clear();
            return;
        }

        std::vector<std::pair<Vec3, Vec3>> portals;
        nav->BuildPortals(triPath, portals);

        if (portals.empty()) {
            StopMovement(session);
            return;
        }

        std::vector<Vec3> smoothPath;
        if (!nav->SmoothPath(cur, targetPos, portals, smoothPath) || smoothPath.size() < 2)
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
    };

    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);

    if (len <= POLICE_SPEED * deltaTime)
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
        dir.z = 0.f;
        len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if (len < 1e-6f)
        {
            StopMovement(session);
            return;
        }
    }

    if (len < 1e-6f)
    {
        StopMovement(session);
        return;
    }

    dir.x /= len;
    dir.y /= len;
    dir.z = 0.f;

    // 위치 갱신
    session.police_state.PositionX += dir.x * POLICE_SPEED * deltaTime;
    session.police_state.PositionY += dir.y * POLICE_SPEED * deltaTime;
    SnapToNavMesh(session);

    session.police_state.VelocityX = dir.x * POLICE_SPEED;
    session.police_state.VelocityY = dir.y * POLICE_SPEED;
    session.police_state.VelocityZ = dir.z * POLICE_SPEED;
    session.police_state.Speed = std::sqrt(
        session.police_state.VelocityX * session.police_state.VelocityX +
        session.police_state.VelocityY * session.police_state.VelocityY
    );

    // state 회전 갱신
    if (len > 1e-3f)
    {
        session.police_state.RotationYaw =
            std::atan2(dir.y, dir.x) * RAD_TO_DEG;
    }
}

static bool HasLineOfSight(SESSION* session, int target_id)
{
    auto it = g_users.find(target_id);
    if (it == g_users.end())
        return false;

    auto target = it->second;
    if (target->state == ST_FREE || !target->isValidSocket()) 
    {
        return false;
    }

    Vec3 start{
        session->police_state.PositionX,
        session->police_state.PositionY,
        session->police_state.PositionZ
    };

    Vec3 end{
        target->cultist_state.PositionX,
        target->cultist_state.PositionY,
        target->cultist_state.PositionZ
    };

    Vec3 dir{
        end.x - start.x,
        end.y - start.y,
        end.z - start.z
    };

    float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (dist < 1e-3f)
        return true;

    dir.x /= dist;
    dir.y /= dist;
    dir.z /= dist;

    Ray ray;
    ray.start = start;
    ray.dir = dir;

    float hitDist;
    int hitTri;

    MAP* map = GetMap(session->room_id);

    // 장애물에 먼저 맞으면 시야 차단
    if (map && map->LineTrace(ray, dist, hitDist, hitTri))
    {
        return false;
    }

    return true;
}

static bool IsCultistTargetAttackable(const SESSION& target)
{
    if (target.role != 0 && target.role != 100)
        return false;

    if (target.state == ST_FREE || target.state == ST_STUN || target.state == ST_DEAD)
        return false;

    if (target.cultist_state.ABP_IsDead || target.cultist_state.ABP_IsStunned)
        return false;

    if (target.role != 100 && !target.isValidSocket())
        return false;

    return true;
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

    if (!HasLineOfSight(ctrl.owner, ctrl.bb.aim_target))
    {
        ctrl.bb.aim_target = -1;
        ctrl.bb.aim_time = 0.f;
        ctrl.owner->police_state.bIsAiming = false;
        return false;
    }

    ctrl.bb.aim_time += dt;
    ctrl.owner->police_state.bIsAiming = true;
    if (ctrl.CanTaser())
        ctrl.owner->police_state.CurrentWeapon = EWeaponType::Taser;
    else if (ctrl.CanPistol())
        ctrl.owner->police_state.CurrentWeapon = EWeaponType::Pistol;

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
    auto it = g_users.find(bb.target_id);
    if (it == g_users.end() || !it->second || !IsCultistTargetAttackable(*it->second))
        return false;

    return bb.target_id != -1 &&
        bb.last_dist_to_target < BATON_RANGE;
}

bool PoliceAIController::CanShoot()
{
    if (bb.aim_target != -1)
    {
        auto it = g_users.find(bb.aim_target);
        return it != g_users.end() && it->second && IsCultistTargetAttackable(*it->second);
    }

    auto it = g_users.find(bb.target_id);
    if (it == g_users.end() || !it->second || !IsCultistTargetAttackable(*it->second))
        return false;

    return bb.target_id != -1 &&
        bb.last_dist_to_target < TASER_RANGE;
}

bool PoliceAIController::CanTaser()
{
    auto it = g_users.find(bb.target_id);
    if (it == g_users.end() || !it->second || !IsCultistTargetAttackable(*it->second))
        return false;

    return bb.target_id != -1 &&
        bb.last_dist_to_target < TASER_RANGE;
}

bool PoliceAIController::CanPistol()
{
    auto it = g_users.find(bb.target_id);
    if (it == g_users.end() || !it->second || !IsCultistTargetAttackable(*it->second))
        return false;

    return bb.target_id != -1 &&
        bb.last_dist_to_target < PISTOL_RANGE;
}

// Action
void PoliceAIController::Patrol(float dt)
{
    NAVMESH* nav = GetNavMesh(owner->room_id);
    if (!nav)
        return;

    Vec3 cur{
        owner->police_state.PositionX,
        owner->police_state.PositionY,
        owner->police_state.PositionZ
    };

    // 목표 없으면 생성
    if (!bb.has_patrol_target)
    {
        int curTri = nav->FindContainingTriangle(cur);
        if (curTri < 0)
            return;

        int randomTri = nav->GetRandomTriangle(curTri, 10);
        if (randomTri < 0)
            return;

        bb.patrol_target = nav->GetTriCenter(randomTri);
        bb.has_patrol_target = true;

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
    if (target->state == ST_FREE ||
        (target->role != 100 && !target->isValidSocket()))
    {
        bb.target_id = -1;
        bb.path.clear();
        return;
    }

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
    owner->police_state.CurrentWeapon = EWeaponType::Baton;
    owner->police_state.bIsAttacking = true;
    StopMovement(*owner);

    HitPacket p{};
    p.TraceStart = {
        owner->police_state.PositionX,
        owner->police_state.PositionY,
        owner->police_state.PositionZ
    };

    float yawRad = owner->police_state.RotationYaw * DEG_TO_RAD;

    p.TraceDir = {
        std::cos(yawRad),
        std::sin(yawRad),
        0.f
    };

    baton_sweep(owner->id, &p);
    BeginBehaviorLock(ATTACK_COOL_DOWN);
}

void PoliceAIController::TaserShoot(float dt)
{
    // 공격 함수 추가
    owner->police_state.CurrentWeapon = EWeaponType::Taser;
    owner->police_state.bIsAiming = false;
    owner->police_state.bIsShooting = true;

    HitPacket p{};
    p.Weapon = EWeaponType::Taser;

    p.TraceStart = {
        owner->police_state.PositionX,
        owner->police_state.PositionY,
        owner->police_state.PositionZ
    };

    float yawRad = owner->police_state.RotationYaw * DEG_TO_RAD;

    p.TraceDir = {
        std::cos(yawRad),
        std::sin(yawRad),
        0.f
    };

    line_trace(owner->id, &p);
    BeginBehaviorLock(ATTACK_COOL_DOWN);
}

void PoliceAIController::PistolShoot(float dt)
{
    // 공격 함수 추가
    owner->police_state.CurrentWeapon = EWeaponType::Pistol;
    owner->police_state.bIsAiming = false;
    owner->police_state.bIsShooting = true;

    HitPacket p{};
    p.Weapon = EWeaponType::Pistol;

    p.TraceStart = {
        owner->police_state.PositionX,
        owner->police_state.PositionY,
        owner->police_state.PositionZ
    };

    float yawRad = owner->police_state.RotationYaw * DEG_TO_RAD;

    p.TraceDir = {
        std::cos(yawRad),
        std::sin(yawRad),
        0.f
    };

    line_trace(owner->id, &p);
    BeginBehaviorLock(ATTACK_COOL_DOWN);
}

static int FindNearbyCultist(int room_id, int self_id)
{
    if (room_id < 0 || room_id >= MAX_ROOM)
        return -1;

    auto selfIt = g_users.find(self_id);
    if (selfIt == g_users.end())
        return -1;

    auto self = selfIt->second;

    Vec3 selfPos{
    self->police_state.PositionX,
    self->police_state.PositionY,
    self->police_state.PositionZ
    };

    const auto& room = g_rooms[room_id].first;

    int best_id = -1;
    float best_dist_sq = VIEW_RANGE_SQ;

    for (int pid : room.player_ids)
    {
        if (pid == -1 || pid == self_id)
            continue;

        auto it = g_users.find(pid);
        if (it == g_users.end())
            continue;

        auto target = it->second;
        if (target->role != 0 && target->role != 100)
            continue;
        if (target->state == ST_FREE)
            continue;

        float dx = target->cultist_state.PositionX - selfPos.x;
        float dy = target->cultist_state.PositionY - selfPos.y;
        float dist_sq = dx * dx + dy * dy;

        if (dist_sq < best_dist_sq)
        {
            best_dist_sq = dist_sq;
            best_id = pid;
        }
    }

    return best_id;
}

// BT
void PoliceAIController::UpdateBlackboard(float dt)
{
    if (dogAI && owner->dog.is_barking)
    {
        if (dogAI->db.target_id != -1)
        {
            bb.target_id = dogAI->db.target_id;
        }
    }
    else
    {
        if (dogAI && bb.target_id == dogAI->db.target_id)
        {
            bb.target_id = -1;
        }
    }

    if (bb.target_id == -1 && !owner->dog.is_barking)
    {
        int found = FindNearbyCultist(owner->room_id, owner->id);
        if (found != -1)
        {
            bb.target_id = found;
        }
    }
    // target 유효성 체크
    if (bb.target_id != -1)
    {
        auto it = g_users.find(bb.target_id);
        if (it == g_users.end())
        {
            bb.target_id = -1;
        }
    }

    // 거리 업데이트
    if (bb.target_id != -1)
    {
        auto it = g_users.find(bb.target_id);
        if (it == g_users.end())
        {
            bb.target_id = -1;
            bb.last_dist_to_target = FLT_MAX;
            return;
        }
        auto target = it->second;
        if (!target || !IsCultistTargetAttackable(*target))
        {
            bb.target_id = -1;
            bb.aim_target = -1;
            bb.aim_time = 0.f;
            bb.last_dist_to_target = FLT_MAX;
            owner->police_state.bIsAiming = false;
            return;
        }

        Vec3 self{
            owner->police_state.PositionX,
            owner->police_state.PositionY,
            owner->police_state.PositionZ
        };

        Vec3 targetPos{
            target->cultist_state.PositionX,
            target->cultist_state.PositionY,
            target->cultist_state.PositionZ
        };

        bb.last_dist_to_target = Dist(self, targetPos);

        if (!owner->dog.is_barking)
        {
            if (bb.last_dist_to_target > CHASE_START_RANGE)
            {
                bb.target_id = -1;
                bb.aim_target = -1;
                bb.aim_time = 0.f;
                owner->police_state.bIsAiming = false;
            }
        }
    }
    else
    {
        bb.last_dist_to_target = FLT_MAX;
    }

    // aim_target 유지 검증
    if (bb.aim_target != -1)
    {
        auto it = g_users.find(bb.aim_target);
        if (it == g_users.end() || !it->second || !IsCultistTargetAttackable(*it->second))
        {
            bb.aim_target = -1;
            bb.aim_time = 0.f;
            owner->police_state.bIsAiming = false;
        }
    }

    if (bb.attack_lock_time <= 0.f)
    {
        owner->police_state.bIsAttacking = false;
        owner->police_state.bIsShooting = false;
    }

    // stuck 판단
    Vec3 cur{
        owner->police_state.PositionX,
        owner->police_state.PositionY,
        owner->police_state.PositionZ
    };

    float moveDist = Dist(cur, bb.lastSnapPos);

    if (moveDist < 1.0f)
    {
        bb.stuck_ticks++;
    }
    else
    {
        bb.stuck_ticks = 0;
    }

    bb.lastSnapPos = cur;

    // stuck 발생 시 처리
    if (bb.stuck_ticks > 30)
    {
        std::cout << "[FIX] stuck -> reset path\n";

        bb.path.clear();
        bb.has_patrol_target = false;
        bb.target_id = -1;
        MoveToNearestTriangle(*owner, cur);
        bb.stuck_ticks = 0;
    }
}

void PoliceAIController::RunBehaviorTree(float dt)
{
    if (root)
        root->Run(*this, dt);
}

void PoliceAIController::Update(float dt)
{
    if (bb.attack_lock_time > 0.f)
    {
        bb.attack_lock_time -= dt;
        if (bb.attack_lock_time < 0.f)
            bb.attack_lock_time = 0.f;

        StopMovement(*owner);

        if (dogAI)
            dogAI->Update(dt);

        return;
    }

    UpdateBlackboard(dt);
    RunBehaviorTree(dt);

    if (dogAI)
        dogAI->Update(dt);
}

void PoliceAIController::BeginBehaviorLock(float lockTime)
{
    bb.attack_lock_time = lockTime;

    bb.path.clear();
    bb.has_patrol_target = false;

    bb.aim_target = -1;
    bb.aim_time = 0.f;
    StopMovement(*owner);
}

// Dog
DogAIController::DogAIController(SESSION* o)
    : AIController{ o }
{
    /*
    Selector
     ├─ Sequence (CanAttack)
     │    └─ Attack
     │
     ├─ Sequence (HasTargetId)
     │    ├─ Selector
     │    │    ├─ Sequence (ShouldStopChaseNode)
     │    │    │    └─ Stop
     │    │    └─ Chase
     │
     ├─ Sequence (NeedFollowOwnerNode)
     │    └─ Follow
     │
     └─ Explore
    */
    auto rootSelector = std::make_unique<Selector>();

    // Chase
    {
        auto seq = std::make_unique<Sequence>();
        seq->children.push_back(std::make_unique<HasTargetIdNode>());

        auto sel = std::make_unique<Selector>();

        // Stop
        {
            auto stopSeq = std::make_unique<Sequence>();
            stopSeq->children.push_back(std::make_unique<ShouldStopChaseNode>());
            stopSeq->children.push_back(std::make_unique<DogStopNode>());
            sel->children.push_back(std::move(stopSeq));
        }

        // Chase
        {
            sel->children.push_back(std::make_unique<DogChaseNode>());
        }

        seq->children.push_back(std::move(sel));
        rootSelector->children.push_back(std::move(seq));
    }

    // Follow
    {
        auto seq = std::make_unique<Sequence>();
        seq->children.push_back(std::make_unique<NeedFollowOwnerNode>());
        seq->children.push_back(std::make_unique<DogFollowNode>());
        rootSelector->children.push_back(std::move(seq));
    }

    // Explore
    {
        rootSelector->children.push_back(std::make_unique<DogExploreNode>());
    }

    root = std::move(rootSelector);
}

void DogAIController::Init()
{
    if (!owner)
        return;

    owner->dog.loc.x = owner->police_state.PositionX + 500.f;
    owner->dog.loc.y = owner->police_state.PositionY;
    owner->dog.loc.z = owner->police_state.PositionZ;
    owner->dog.owner = owner->id;
    owner->dog.is_barking = false;

    db.lastTargetPos = {
        static_cast<float>(owner->dog.loc.x),
        static_cast<float>(owner->dog.loc.y),
        static_cast<float>(owner->dog.loc.z)
    };
    db.repath_timer = 0.f;
    db.path.clear();

    bInitialized = true;
}

static void MoveAlongPathDog(DogAIController& dogAI, const Vec3& targetPos, float dt)
{
    SESSION* session = dogAI.owner;
    if (!session)
        return;

    Vec3 cur{
        static_cast<float>(session->dog.loc.x),
        static_cast<float>(session->dog.loc.y),
        static_cast<float>(session->dog.loc.z)
    };

    if (Dist(cur, targetPos) <= ARRIVE_RANGE)
    {
        return;
    }

    float dx = targetPos.x - dogAI.db.lastTargetPos.x;
    float dy = targetPos.y - dogAI.db.lastTargetPos.y;
    float dz = targetPos.z - dogAI.db.lastTargetPos.z;
    float dist2 = dx * dx + dy * dy + dz * dz;

    if (dist2 > REPATH_DIST * REPATH_DIST)
    {
        dogAI.db.lastTargetPos = targetPos;
        dogAI.db.path.clear();
    }

    if (dogAI.db.path.empty() && dogAI.db.repath_timer > 0.3f)
    {
        NAVMESH* nav = GetNavMesh(session->room_id);
        if (!nav)
            return;

        std::vector<int> triPath;
        if (!nav->FindTriPath(cur, targetPos, triPath))
        {
            dogAI.db.path.clear();
            return;
        }

        std::vector<std::pair<Vec3, Vec3>> portals;
        nav->BuildPortals(triPath, portals);

        if (portals.empty())
            return;

        std::vector<Vec3> smoothPath;
        if (!nav->SmoothPath(cur, targetPos, portals, smoothPath) || smoothPath.size() < 2)
            return;

        dogAI.db.path = smoothPath;
        dogAI.db.repath_timer = 0.f;
    }

    if (dogAI.db.path.empty())
        return;

    Vec3 next = (dogAI.db.path.size() >= 2) ? dogAI.db.path[1] : dogAI.db.path[0];

    Vec3 dir{
        next.x - cur.x,
        next.y - cur.y,
        next.z - cur.z
    };

    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

    if (len <= DOG_SPEED * dt)
    {
        dogAI.db.path.erase(dogAI.db.path.begin());

        if (dogAI.db.path.empty())
            return;

        next = dogAI.db.path[0];

        dir.x = next.x - cur.x;
        dir.y = next.y - cur.y;
        dir.z = next.z - cur.z;
        len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    }

    if (len < 1e-3f)
        return;

    dir.x /= len;
    dir.y /= len;
    dir.z /= len;

    session->dog.loc.x += dir.x * DOG_SPEED * dt;
    session->dog.loc.y += dir.y * DOG_SPEED * dt;
    session->dog.loc.z += dir.z * DOG_SPEED * dt;

    session->dog.rot.yaw = std::atan2(dir.y, dir.x) * RAD_TO_DEG;
}

static int FindNearbyCultistForDog(int room_id, const Vec3& dogPos)
{
    if (room_id < 0 || room_id >= MAX_ROOM)
        return -1;

    const auto& room = g_rooms[room_id].first;

    int best_id = -1;
    float best_dist_sq = VIEW_RANGE_SQ;

    for (int pid : room.player_ids)
    {
        if (pid == -1)
            continue;

        auto it = g_users.find(pid);
        if (it == g_users.end() || !it->second)
            continue;

        auto target = it->second;
        if (!IsCultistTargetAttackable(*target))
            continue;

        float dx = target->cultist_state.PositionX - dogPos.x;
        float dy = target->cultist_state.PositionY - dogPos.y;
        float dz = target->cultist_state.PositionZ - dogPos.z;
        float dist_sq = dx * dx + dy * dy + dz * dz;

        if (dist_sq < best_dist_sq)
        {
            best_dist_sq = dist_sq;
            best_id = pid;
        }
    }

    return best_id;
}

// Condition Node
bool HasTargetIdNode::Run(AIController& ai, float)
{
    return static_cast<DogAIController&>(ai).HasTargetId();
}

bool NeedFollowOwnerNode::Run(AIController& ai, float)
{
    return static_cast<DogAIController&>(ai).NeedFollowOwner();
}

bool ShouldStopChaseNode::Run(AIController& ai, float)
{
    return static_cast<DogAIController&>(ai).ShouldStopChase();
}

// Action Node
bool DogChaseNode::Run(AIController& ai, float dt)
{
    static_cast<DogAIController&>(ai).Chase(dt);
    return true;
}

bool DogStopNode::Run(AIController& ai, float dt)
{
    static_cast<DogAIController&>(ai).Stop(dt);
    return true;
}

bool DogFollowNode::Run(AIController& ai, float dt)
{
    static_cast<DogAIController&>(ai).Follow(dt);
    return true;
}

bool DogExploreNode::Run(AIController& ai, float dt)
{
    static_cast<DogAIController&>(ai).Explore(dt);
    return true;
}

// Condition
bool DogAIController::NeedFollowOwner()
{
    return db.bNeedFollowOwner;
}

bool DogAIController::ShouldStopChase()
{
    return db.bStopChaseForOwnerDist;
}

bool DogAIController::HasTargetId()
{
    if (db.target_id == -1)
        return false;

    auto it = g_users.find(db.target_id);
    if (it == g_users.end() || !it->second)
        return false;

    if (!IsCultistTargetAttackable(*it->second))
        return false;

    return true;
}

// Action
void DogAIController::Chase(float dt)
{
    if (db.target_id == -1)
        return;

    owner->dog.is_barking = true;
    MoveAlongPathDog(*this, db.targetPos, dt);
}

void DogAIController::Stop(float dt)
{
    if (db.target_id == -1)
        return;

    Vec3 dogPos{
        static_cast<float>(owner->dog.loc.x),
        static_cast<float>(owner->dog.loc.y),
        static_cast<float>(owner->dog.loc.z)
    };

    Vec3 dir{
        db.targetPos.x - dogPos.x,
        db.targetPos.y - dogPos.y,
        db.targetPos.z - dogPos.z,
    };

    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

    if (len > 1e-3f)
    {
        owner->dog.rot.yaw = std::atan2(dir.y, dir.x) * RAD_TO_DEG;
    }
    // 이동 없음
}

void DogAIController::Follow(float dt)
{
    Vec3 policePos{
        owner->police_state.PositionX,
        owner->police_state.PositionY,
        owner->police_state.PositionZ
    };

    owner->dog.is_barking = false;
    MoveAlongPathDog(*this, policePos, dt);
}

void DogAIController::Explore(float dt)
{
    if (db.path.empty())
    {
        Vec3 policePos{
            owner->police_state.PositionX,
            owner->police_state.PositionY,
            owner->police_state.PositionZ
        };

        db.targetPos = {
            policePos.x + (rand() % 1601 - 800),
            policePos.y + (rand() % 1601 - 800),
            policePos.z
        };
    }

    owner->dog.is_barking = false;
    MoveAlongPathDog(*this, db.targetPos, dt);
}

void DogAIController::UpdateBlackboard(float dt)
{
    Vec3 dogPos{
        static_cast<float>(owner->dog.loc.x),
        static_cast<float>(owner->dog.loc.y),
        static_cast<float>(owner->dog.loc.z)
    };

    Vec3 policePos{
        owner->police_state.PositionX,
        owner->police_state.PositionY,
        owner->police_state.PositionZ
    };

    // 경찰 거리
    db.dist_to_owner = Dist(dogPos, policePos);

    int found = FindNearbyCultistForDog(owner->room_id, dogPos);
    if (found != -1)
    {
        db.target_id = found;
    }

    // target 유효성 체크
    if (db.target_id != -1)
    {
        auto it = g_users.find(db.target_id);
        if (it == g_users.end() || !it->second || !IsCultistTargetAttackable(*it->second))
        {
            db.target_id = -1;
            db.dist_to_target = FLT_MAX;

            db.path.clear();
            owner->dog.is_barking = false;
        }
        else
        {
            auto target = it->second;

            Vec3 targetPos{
                target->cultist_state.PositionX,
                target->cultist_state.PositionY,
                target->cultist_state.PositionZ
            };

            // target 위치 저장
            db.targetPos = targetPos;
            // target 거리
            db.dist_to_target = Dist(dogPos, targetPos);
        }
    }
    else
    {
        db.dist_to_target = FLT_MAX;
    }

    // 타겟 없을 때 follow
    if (db.target_id == -1)
    {
        if (db.dist_to_owner >= DOG_MAX_DIST)
            db.bNeedFollowOwner = true;
        else if (db.dist_to_owner <= FOLLOW_MAX_DIST)
            db.bNeedFollowOwner = false;
    }
    else
    {
        db.bNeedFollowOwner = false;
    }

    // 타겟 있을 때 chase, stop
    if (db.target_id != -1)
    {
        if (db.dist_to_owner >= DOG_MAX_DIST)
            db.bStopChaseForOwnerDist = true;
        else if (db.dist_to_owner <= FOLLOW_MAX_DIST)
            db.bStopChaseForOwnerDist = false;
    }
    else
    {
        db.bStopChaseForOwnerDist = false;
    }

    // repath 타이머
    db.repath_timer += dt;
}

void DogAIController::RunBehaviorTree(float dt)
{
    if (root)
        root->Run(*this, dt);
}

void DogAIController::Update(float dt)
{
    if (!owner)
        return;
    if (!bInitialized)
        Init();

    UpdateBlackboard(dt);
    RunBehaviorTree(dt);
}