#define NOMINMAX
#include "CultistAI.h"
#include <chrono>
#include <thread>
#include <array>
#include <cmath>
#include <concurrent_priority_queue.h>
#include <concurrent_unordered_set.h>
#include <concurrent_unordered_map.h>
#include <random>
#include <mutex>
#include <queue>
#include "map.h"
#include "MapManager.h"

using namespace std;
extern concurrency::concurrent_unordered_map<int, std::shared_ptr<SESSION>> g_users;
extern concurrency::concurrent_unordered_set<int> g_cultist_ai_ids;
extern std::array<std::pair<Room, MAPTYPE>, MAX_ROOM> g_rooms;
extern concurrency::concurrent_priority_queue<TIMER_EVENT> timer_queue;
extern std::array<std::array<Altar, ALTAR_PER_ROOM>, MAX_ROOM> g_altars;
extern std::mutex g_room_mtx;
extern std::queue<RoomTask> g_room_q;
extern std::condition_variable g_room_cv;
extern std::vector<int> free_session_ids;
extern std::mutex free_id_mtx;

void AddCutltistAi(int ai_id, uint8_t ai_role, int room_id)
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

    auto [it_ai, inserted] = g_cultist_ai_ids.insert(ai_id);

    auto& room = g_rooms[room_id];
    for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i)
    {
        if (room.first.player_ids[i] == -1)
        {
            room.first.player_ids[i] = ai_id;
            room.first.cultist += 1;
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

void KillCultistAi(int ai_id)
{
    auto it = g_users.find(ai_id);
    if (it == g_users.end())
        return;

    auto session = it->second;

    if (session->role != 100)
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
    session.cultist_state.VelocityX = 0.f;
    session.cultist_state.VelocityY = 0.f;
    session.cultist_state.VelocityZ = 0.f;
    session.cultist_state.Speed = 0.f;
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

    auto* cultistAI = dynamic_cast<CultistAIController*>(session.ai.get());
    if (!cultistAI)
        return;

    float dx = targetPos.x - cultistAI->bb.lastTargetPos.x;
    float dy = targetPos.y - cultistAI->bb.lastTargetPos.y;
    float dist2 = dx * dx + dy * dy;

    if (dist2 > REPATH_DIST * REPATH_DIST)
    {
        // 타겟이 충분히 이동, 경로 무효화
        cultistAI->bb.lastTargetPos = targetPos;
        cultistAI->bb.path.clear();
    }

    if (cultistAI->bb.path.empty())
    {
        NAVMESH* nav = GetNavMesh(session.room_id);
        if (!nav) 
            return;

        std::vector<int> triPath;
        if (!nav->FindTriPath(cur, targetPos, triPath))
        {
            StopMovement(session);
            cultistAI->bb.path.clear();
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
        cultistAI->bb.path = smoothPath;
    }

    if (cultistAI->bb.path.empty())
    {
        StopMovement(session);
        return;
    }

    if (cultistAI->bb.path.size() < 1)
    {
        std::cout << "[FATAL] path empty before next selection\n";
        StopMovement(session);
        return;
    }

    // 다음 목표 노드
    Vec3 next;
    if (cultistAI->bb.path.size() >= 2) {
        next = cultistAI->bb.path[1];
    }
    else {
        next = cultistAI->bb.path[0];
    }

    Vec3 dir{
        next.x - cur.x,
        next.y - cur.y,
        0.f
        // next.z - cur.z
    };
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    // float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len <= CULTIST_SPEED * deltaTime)
    {
        cultistAI->bb.path.erase(cultistAI->bb.path.begin());

        if (cultistAI->bb.path.empty())
        {
            StopMovement(session);
            return;
        }

        next = cultistAI->bb.path[0];

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
    session.cultist_state.PositionX += dir.x * CULTIST_SPEED * deltaTime;
    session.cultist_state.PositionY += dir.y * CULTIST_SPEED * deltaTime;
    // ai.cultist_state.PositionZ += dir.z * CULTIST_SPEED * deltaTime;

    session.cultist_state.VelocityX = dir.x * CULTIST_SPEED;
    session.cultist_state.VelocityY = dir.y * CULTIST_SPEED;
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

static int FindAnyCultist(int room_id, int self_id)
{
    if (room_id < 0 || room_id >= MAX_ROOM)
        return -1;

    const auto& room = g_rooms[room_id].first;

    for (int pid : room.player_ids)
    {
        if (pid == -1 || pid == self_id)
            continue;
        if (g_cultist_ai_ids.count(pid))
            continue;

        auto it = g_users.find(pid);
        if (it == g_users.end())
            continue;

        auto target = it->second;
        if (target->state == ST_FREE)
            continue;
        if (target->room_id != room_id)
            continue;
        if (!target->isValidSocket())
            continue;
        if (target->role != 0)
            continue;

        return pid;
    }
    return -1;
}

static int FindNearbyPolice(int room_id, int self_id)
{
    if (room_id < 0 || room_id >= MAX_ROOM)
        return -1;

    auto selfIt = g_users.find(self_id);
    if (selfIt == g_users.end())
        return -1;

    auto self = selfIt->second;

    Vec3 selfPos{
        self->cultist_state.PositionX,
        self->cultist_state.PositionY,
        self->cultist_state.PositionZ
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

        if (target->state == ST_FREE)
            continue;

        if (target->room_id != room_id)
            continue;

        if (target->role != 101 && !target->isValidSocket())
            continue;

        float dx = target->police_state.PositionX - selfPos.x;
        float dy = target->police_state.PositionY - selfPos.y;
        float dist_sq = dx * dx + dy * dy;

        if (dist_sq < best_dist_sq)
        {
            best_dist_sq = dist_sq;
            best_id = pid;
        }
    }

    return best_id;
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
        self->cultist_state.PositionX,
        self->cultist_state.PositionY,
        self->cultist_state.PositionZ
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
        if (target->role != 0)
            continue;
        if (target->state == ST_FREE)
            continue;
        if (!target->isValidSocket())
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

static bool IsNearAltar(SESSION& session)
{
    int room_id = session.room_id;
    if (room_id < 0 || room_id >= MAX_ROOM)
        return false;

    auto aiPtr = session.ai;
    auto* cultistAI = dynamic_cast<CultistAIController*>(aiPtr.get());
    if (!cultistAI)
        return false;

    Vec3 selfPos{
        session.cultist_state.PositionX,
        session.cultist_state.PositionY,
        session.cultist_state.PositionZ
    };


    for (int i = 0; i < ALTAR_PER_ROOM; ++i)
    {
        Altar& altar = g_altars[room_id][i];

        if (altar.isActivated)
            continue;

        if (altar.gauge >= 100)
            continue;

        float dx = selfPos.x - (float)altar.loc.x;
        float dy = selfPos.y - (float)altar.loc.y;
        float dist2 = dx * dx + dy * dy;


        if (dist2 <= ALTAR_TRIGGER_RANGE_SQ)
        {
            cultistAI->bb.ritual_id = i;
            return true;
        }
    }

    cultistAI->bb.ritual_id = -1;
    return false;
}

std::optional<std::pair<FVector, FRotator>> GetMovePoint(int c_id, int targetId) {
    auto itHealer = g_users.find(c_id);
    auto itTarget = g_users.find(targetId);
    if (itHealer == g_users.end() || itTarget == g_users.end()) {
        return std::nullopt;
    }

    const auto healer = itHealer->second;
    const auto target = itTarget->second;

    FVector mid{
         static_cast<double>((healer->cultist_state.PositionX + target->cultist_state.PositionX) * 0.5),
         static_cast<double>((healer->cultist_state.PositionY + target->cultist_state.PositionY) * 0.5),
         static_cast<double>((healer->cultist_state.PositionZ + target->cultist_state.PositionZ) * 0.5)
    };

    const double dx = static_cast<double>(target->cultist_state.PositionX - healer->cultist_state.PositionX);
    const double dy = static_cast<double>(target->cultist_state.PositionY - healer->cultist_state.PositionY);

    double yawHealer = std::atan2(dy, dx) * 180.0 / PI;
    if (yawHealer < 0.0) {
        yawHealer += 360.0;
    }
    FRotator rot{ 0.0, yawHealer, 0.0 };

    return std::make_pair(mid, rot);
}

void CultistAIWorkerLoop()
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

        for (int ai_id : g_cultist_ai_ids)
        {
            auto it = g_users.find(ai_id);
            if (it == g_users.end())
                continue;

            auto session = it->second;
            if (session->role != 100)
                continue;

            auto aiPtr = session->ai;
            if (!aiPtr)
                continue;

            auto* cultistAI = dynamic_cast<CultistAIController*>(aiPtr.get());
            if (!cultistAI)
                continue;

            if (cultistAI->bb.ai_state == AIState::Free)
                continue;

            bool canMove = true;
            auto& st = session->cultist_state;
            if (!cultistAI->CanMove())
            {
                StopMovement(*session);
                canMove = false;
            }

            if (canMove)
            {
                session->ai->Update(dt);
            }

            CultistPacket packet{};
            packet.header = cultistHeader;
            packet.size = sizeof(CultistPacket);
            packet.state = session->cultist_state;
            broadcast_in_room(*session, &packet, VIEW_RANGE);
        }

        nextTick += std::chrono::duration_cast<clock::duration>(
            std::chrono::duration<float>(fixed_dt));

        std::this_thread::sleep_until(nextTick);
    }
}

// CultistAIController
CultistAIController::CultistAIController(SESSION* o)
    : AIController(o)
{
    auto rootSelector = std::make_unique<Selector>();

    // Runaway
    {
        auto seq = std::make_unique<Sequence>();
        seq->children.push_back(std::make_unique<CanRunawayNode>());
        seq->children.push_back(std::make_unique<RunawayNode>());
        rootSelector->children.push_back(std::move(seq));
    }

    // Chase + Heal
    {
        auto seq = std::make_unique<Sequence>();
        seq->children.push_back(std::make_unique<CanChaseNode>());

        auto chaseSelector = std::make_unique<Selector>();

        // Heal
        {
            auto healSeq = std::make_unique<Sequence>();
            healSeq->children.push_back(std::make_unique<CanHealNode>());
            healSeq->children.push_back(std::make_unique<HealNode>());
            chaseSelector->children.push_back(std::move(healSeq));
        }

        // 기본 Chase
        chaseSelector->children.push_back(std::make_unique<ChaseNode>());

        seq->children.push_back(std::move(chaseSelector));
        rootSelector->children.push_back(std::move(seq));
    }

    // Ritual
    {
        auto seq = std::make_unique<Sequence>();
        seq->children.push_back(std::make_unique<CanRitualNode>());
        seq->children.push_back(std::make_unique<RitualNode>());
        rootSelector->children.push_back(std::move(seq));
    }

    // Patrol
    rootSelector->children.push_back(std::make_unique<PatrolNode>());

    root = std::move(rootSelector);
}

// Condition Node
bool CanRunawayNode::Run(AIController& ai, float)
{
    return static_cast<CultistAIController&>(ai).CanRunaway();
}

bool CanChaseNode::Run(AIController& ai, float)
{
    return static_cast<CultistAIController&>(ai).CanChase();
}

bool CanHealNode::Run(AIController& ai, float)
{
    return static_cast<CultistAIController&>(ai).CanHeal();
}

bool CanRitualNode::Run(AIController& ai, float)
{
    return static_cast<CultistAIController&>(ai).CanRitual();
}

// Action Node
bool RunawayNode::Run(AIController& ai, float dt)
{
    static_cast<CultistAIController&>(ai).Runaway(dt);
    return true;
}

bool ChaseNode::Run(AIController& ai, float dt)
{
    static_cast<CultistAIController&>(ai).Chase(dt);
    return true;
}

bool HealNode::Run(AIController& ai, float dt)
{
    static_cast<CultistAIController&>(ai).Heal(dt);
    return true;
}

bool RitualNode::Run(AIController& ai, float dt)
{
    static_cast<CultistAIController&>(ai).Ritual(dt);
    return true;
}

bool PatrolNode::Run(AIController& ai, float dt)
{
    static_cast<CultistAIController&>(ai).Patrol(dt);
    return true;
}

// Condition
bool CultistAIController::CanRunaway()
{
    return bb.runaway_id != -1;
}

bool CultistAIController::CanChase()
{
    return bb.target_id != -1;
}

bool CultistAIController::CanHeal()
{
    if (bb.target_id == -1)
        return false;

    if (bb.last_dist_to_target > HEAL_DIST)
        return false;

    auto it = g_users.find(bb.target_id);
    if (it == g_users.end() || !it->second)
        return false;

    auto target = it->second;
    if (target->cultist_state.CurrentHealth > 50.f)
        return false;

    if (owner->cultist_state.ABP_DoHeal ||
        target->cultist_state.ABP_GetHeal)
        return false;

    return true;
}

bool CultistAIController::CanRitual()
{
    return bb.ritual_id != -1;
}

// Action
void CultistAIController::Patrol(float dt)
{
    NAVMESH* nav = GetNavMesh(owner->room_id);
    if (!nav)
        return;

    Vec3 cur{
        owner->cultist_state.PositionX,
        owner->cultist_state.PositionY,
        owner->cultist_state.PositionZ
    };

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
        bb.stuck_ticks = 0;
        bb.last_dist_to_target = FLT_MAX;
        bb.path.clear();
    }

    float dist = Dist(cur, bb.patrol_target);

    if (dist < CHASE_STOP_RANGE)
    {
        bb.has_patrol_target = false;
        bb.path.clear();
        return;
    }

    if (dist > bb.last_dist_to_target - STUCK_RANGE)
        bb.stuck_ticks++;
    else
        bb.stuck_ticks = 0;

    bb.last_dist_to_target = dist;

    if (bb.stuck_ticks > MAX_STUCK_TICK)
    {
        bb.has_patrol_target = false;
        bb.path.clear();
        return;
    }

    MoveAlongPath(*owner, bb.patrol_target, dt);
}

void CultistAIController::Chase(float dt)
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
        owner->cultist_state.PositionX,
        owner->cultist_state.PositionY,
        owner->cultist_state.PositionZ
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

void CultistAIController::Runaway(float dt)
{
    MAP* map = GetMap(owner->room_id);
    NAVMESH* nav = GetNavMesh(owner->room_id);
    if (!map || !nav)
        return;

    Vec3 selfPos{
        owner->cultist_state.PositionX,
        owner->cultist_state.PositionY,
        owner->cultist_state.PositionZ
    };

    if (bb.has_runaway_target && bb.runaway_ticks < 30 &&
        Dist(selfPos, bb.runaway_pos) > ARRIVE_RANGE)
    {
        bb.runaway_ticks++;
        MoveAlongPath(*owner, bb.runaway_pos, dt);
        return;
    }

    int police_id = bb.runaway_id;
    if (police_id < 0)
        return;

    auto it = g_users.find(police_id);
    if (it == g_users.end())
        return;

    Vec3 policePos{
        it->second->police_state.PositionX,
        it->second->police_state.PositionY,
        it->second->police_state.PositionZ
    };

    // 후보 계산
    Vec3 bestPos = selfPos;
    float bestScore = -FLT_MAX;

    int selfTri = nav->FindContainingTriangle(selfPos);
    if (selfTri < 0)
        return;

    std::vector<RunawayCandidate> candidates;
    candidates.reserve(sampleCount);

    static std::default_random_engine dre{ std::random_device()() };
    std::uniform_real_distribution<float> angleDist(0.f, 2.f * PI);

    for (int i = 0; i < sampleCount; ++i)
    {
        float angle = angleDist(dre);

        Vec3 candidate{
            selfPos.x + std::cos(angle) * sampleRadius,
            selfPos.y + std::sin(angle) * sampleRadius,
            selfPos.z
        };

        int tri = nav->FindContainingTriangle(candidate);
        if (tri < 0)
            continue;

        float score = 0.f;

        float distPolice = Dist(candidate, policePos);
        float distScore = std::min(distPolice / (sampleRadius * 2.f), 1.f);
        score += distScore * 1000.f * 0.30f;

        candidates.push_back({ candidate, score, tri });
    }

    std::sort(candidates.begin(), candidates.end());

    for (int i = 0; i < std::min(3, (int)candidates.size()); ++i)
    {
        std::vector<int> triPath;
        if (!nav->FindTriPath(selfPos, candidates[i].pos, triPath))
            continue;

        float pathScore = std::min((float)triPath.size() / 50.f, 1.f);
        float finalScore = candidates[i].score + pathScore * 1000.f * 0.35f;

        if (finalScore > bestScore)
        {
            bestScore = finalScore;
            bestPos = candidates[i].pos;
        }
    }

    if (!bb.has_runaway_target)
    {
        bb.runaway_pos = bestPos;
        bb.has_runaway_target = true;
        bb.runaway_ticks = 0;
        bb.path.clear();
    }

    MoveAlongPath(*owner, bb.runaway_pos, dt);
}

void CultistAIController::Heal(float dt)
{
    if (bb.target_id < 0)
        return;

    auto it = g_users.find(bb.target_id);
    if (it == g_users.end())
        return;

    auto target = it->second;

    if (owner->cultist_state.ABP_DoHeal || target->cultist_state.ABP_GetHeal)
    {
        StopMovement(*owner);
        return;
    }

    auto moveOpt = GetMovePoint(owner->id, bb.target_id);
    if (!moveOpt)
        return;

    auto [moveLoc, moveRot] = *moveOpt;

    Vec3 selfPos{
        owner->cultist_state.PositionX,
        owner->cultist_state.PositionY,
        owner->cultist_state.PositionZ
    };

    Vec3 healMovePos{
        (float)moveLoc.x,
        (float)moveLoc.y,
        (float)moveLoc.z
    };

    float dist = Dist(selfPos, healMovePos);

    if (dist > HEAL_DIST)
    {
        MoveAlongPath(*owner, healMovePos, dt);
        return;
    }

    // Heal 시작
    owner->cultist_state.ABP_DoHeal = 1;
    owner->heal_partner = bb.target_id;
    bb.path.clear();
}

void CultistAIController::Ritual(float dt)
{
    if (bb.ritual_id < 0)
        return;

    Altar& altar = g_altars[owner->room_id][bb.ritual_id];

    Vec3 altarPos{
        (float)altar.loc.x,
        (float)altar.loc.y,
        (float)altar.loc.z
    };

    MoveAlongPath(*owner, altarPos, dt);

    if (!bb.path.empty())
        return;

    StopMovement(*owner);

    if (!altar.isActivated)
    {
        altar.isActivated = true;
        altar.time = std::chrono::system_clock::now();
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - altar.time).count();

    if (elapsed > 0)
    {
        int add = elapsed / 100;
        if (add > 0)
        {
            altar.gauge = std::min(100, altar.gauge + add);
            altar.time = now;
        }
    }

    if (altar.gauge >= 100)
    {
        altar.isActivated = false;
        bb.ritual_id = -1;
    }
}

bool CultistAIController::CanMove() const
{
    const auto& st = owner->cultist_state;

    if (owner->state == ST_STUN || owner->state == ST_DEAD)
        return false;

    if (st.ABP_IsDead || st.ABP_IsStunned || st.ABP_IsHitByAnAttack)
        return false;

    return true;
}

void CultistAIController::ApplyBatonHit(const Vec3& attackerPos)
{
    auto& st = owner->cultist_state;

    if (!CanMove())
        return;

    st.ABP_IsHitByAnAttack = 1;
    st.ABP_IsPerforming = 0;
    st.ABP_DoHeal = 0;
    st.ABP_GetHeal = 0;

    bb.path.clear();
    bb.target_id = -1;
    bb.has_patrol_target = false;
    bb.has_runaway_target = false;
    bb.ritual_id = -1;

    Vec3 cur{
        owner->cultist_state.PositionX,
        owner->cultist_state.PositionY,
        owner->cultist_state.PositionZ
    };

    Vec3 dir{
        cur.x - attackerPos.x,
        cur.y - attackerPos.y,
        0.f
    };

    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len < 1e-3f)
        return;

    dir.x /= len;
    dir.y /= len;

    owner->cultist_state.PositionX += dir.x * pushDist;
    owner->cultist_state.PositionY += dir.y * pushDist;
    owner->cultist_state.RotationYaw =
        std::atan2(dir.y, dir.x) * RAD_TO_DEG;

    StopMovement(*owner);

    st.CurrentHealth -= 100.f;

    if (st.CurrentHealth <= 0.f)
    {
        if (st.ABP_IsStunned)
        {
            st.ABP_IsDead = 1;
            owner->state = ST_DEAD;
            bb.ai_state = AIState::Die;
        }
        else
        {
            st.ABP_IsStunned = 1;
            st.ABP_TTStun = 1;
            owner->state = ST_STUN;
            bb.ai_state = AIState::Stun;

            TIMER_EVENT ev;
            ev.c_id = owner->id;
            ev.target_id = owner->id;
            ev.event_id = EV_STUN;
            ev.wakeup_time = std::chrono::system_clock::now() + 10s;

            timer_queue.push(ev);
        }
    }
}

// BT
void CultistAIController::UpdateBlackboard(float dt)
{
    int police_id = FindNearbyPolice(owner->room_id, owner->id);
    bb.runaway_id = police_id;

    int target = FindNearbyCultist(owner->room_id, owner->id);
    bb.target_id = target;

    if (bb.target_id != -1)
    {
        auto it = g_users.find(bb.target_id);
        if (it == g_users.end() || !it->second)
        {
            bb.target_id = -1;
            bb.last_dist_to_target = FLT_MAX;
        }
        else
        {
            auto target = it->second;

            Vec3 self{
                owner->cultist_state.PositionX,
                owner->cultist_state.PositionY,
                owner->cultist_state.PositionZ
            };

            Vec3 targetPos{
                target->cultist_state.PositionX,
                target->cultist_state.PositionY,
                target->cultist_state.PositionZ
            };

            bb.last_dist_to_target = Dist(self, targetPos);
        }
    }
    else
    {
        bb.last_dist_to_target = FLT_MAX;
    }

    bb.ritual_id = -1;

    for (int i = 0; i < ALTAR_PER_ROOM; ++i)
    {
        Altar& altar = g_altars[owner->room_id][i];

        if (altar.isActivated || altar.gauge >= 100)
            continue;

        float dx = owner->cultist_state.PositionX - (float)altar.loc.x;
        float dy = owner->cultist_state.PositionY - (float)altar.loc.y;
        float dist2 = dx * dx + dy * dy;

        if (dist2 <= ALTAR_TRIGGER_RANGE_SQ)
        {
            bb.ritual_id = i;
            break;
        }
    }

    Vec3 cur{
        owner->cultist_state.PositionX,
        owner->cultist_state.PositionY,
        owner->cultist_state.PositionZ
    };

    float moveDist = Dist(cur, bb.lastSnapPos);

    if (moveDist < 1.0f)
        bb.stuck_ticks++;
    else
        bb.stuck_ticks = 0;

    bb.lastSnapPos = cur;

    if (bb.stuck_ticks > 30)
    {
        bb.path.clear();
        bb.has_patrol_target = false;
        bb.has_runaway_target = false;
        bb.target_id = -1;
        bb.runaway_id = -1;
        bb.stuck_ticks = 0;
    }
}

void CultistAIController::RunBehaviorTree(float dt)
{
    if (root)
        root->Run(*this, dt);
}

void CultistAIController::Update(float dt)
{
    UpdateBlackboard(dt);
    RunBehaviorTree(dt);
}