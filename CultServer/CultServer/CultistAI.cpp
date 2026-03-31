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

    BroadcastCultistAIState(*session, &pkt);
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
    BroadcastCultistAIState(*session, &pkt);
    
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
    const float speed = 300.f; // 600cm/s
    if (len <= speed * deltaTime)
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

        if (target->role != 1)
            continue;

        if (!target->isValidSocket())
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
        if (g_cultist_ai_ids.count(pid))
            continue;

        auto it = g_users.find(pid);
        if (it == g_users.end())
            continue;

        auto target = it->second;
        if (target->role != 0)
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

static void UpdateAIState(SESSION& session)
{
    auto aiPtr = session.ai;
    auto* cultistAI = dynamic_cast<CultistAIController*>(aiPtr.get());
    if (!cultistAI)
        return;

    // 경찰 발견 Runaway
    int police_id = FindNearbyPolice(session.room_id, session.id);
    if (police_id >= 0)
    {
        if (cultistAI->bb.ai_state == AIState::Heal && session.heal_partner >= 0)
        {
            auto it = g_users.find(session.heal_partner);
            if (it != g_users.end())
            {
                NoticePacket packet;
                packet.header = endHealHeader;
                packet.size = sizeof(NoticePacket);

                it->second->do_send_packet(&packet);
            }

            // Heal 상태 해제
            session.cultist_state.ABP_DoHeal = 0;
            session.cultist_state.ABP_GetHeal = 0;
            session.heal_partner = -1;
        }
        else if (cultistAI->bb.ai_state == AIState::Ritual)
        {
            Altar& altar = g_altars[session.room_id][cultistAI->bb.ritual_id];
            altar.isActivated = false;
            cultistAI->bb.ritual_id = -1;
        }
        cultistAI->bb.ai_state = AIState::Runaway;
        cultistAI->bb.target_id = police_id;
        cultistAI->bb.has_patrol_target = false;
        cultistAI->bb.path.clear();
        return;
    }
    if (cultistAI->bb.ai_state == AIState::Runaway) {
        cultistAI->bb.has_runaway_target = false;
        cultistAI->bb.runaway_ticks = 0;
    }
    // 치료 중
    if (cultistAI->bb.ai_state == AIState::Heal)
    {
        if (!session.cultist_state.ABP_DoHeal &&
            !session.cultist_state.ABP_GetHeal)
        {
            cultistAI->bb.ai_state = AIState::Patrol;
            cultistAI->bb.target_id = -1;
            cultistAI->bb.path.clear();
        }
        StopMovement(session);
        return;
    }
    // 제단 진행 중
    if (cultistAI->bb.ai_state == AIState::Ritual)
    {
        if (cultistAI->bb.ritual_id < 0)
        {
            cultistAI->bb.ai_state = AIState::Patrol;
            return;
        }
        return;
    }

    // 제단 발견 Ritual
    if (IsNearAltar(session))
    {
        cultistAI->bb.ai_state = AIState::Ritual;
        cultistAI->bb.has_patrol_target = false;
        cultistAI->bb.path.clear();
        return;
    }

    // 이미 Chase중
    if (cultistAI->bb.ai_state == AIState::Chase && cultistAI->bb.target_id >= 0)
    {
        auto it = g_users.find(cultistAI->bb.target_id);
        if (it == g_users.end())
        {
            cultistAI->bb.ai_state = AIState::Patrol;
            cultistAI->bb.target_id = -1;
            return;
        }
        return;
    }

    // 유저 발견 Chase
    int cultist_id = FindNearbyCultist(session.room_id, session.id);
    if (cultist_id >= 0)
    {
        auto it = g_users.find(cultist_id);
        if (it != g_users.end())
        {
            Vec3 selfPos{
               session.cultist_state.PositionX,
               session.cultist_state.PositionY,
               session.cultist_state.PositionZ
            };
            Vec3 targetPos{
                it->second->cultist_state.PositionX,
                it->second->cultist_state.PositionY,
                it->second->cultist_state.PositionZ
            };

            float dist = Dist(selfPos, targetPos);
            if (dist <= CHASE_START_RANGE)
            {
                cultistAI->bb.ai_state = AIState::Chase;
                cultistAI->bb.target_id = cultist_id;
                cultistAI->bb.has_patrol_target = false;
                cultistAI->bb.path.clear();
                return;
            }
        }
    }

    // 기본 상태
    cultistAI->bb.ai_state = AIState::Patrol;
}

static void ExecuteAIState(SESSION& session, float dt) 
{
    auto aiPtr = session.ai;
    auto* cultistAI = dynamic_cast<CultistAIController*>(aiPtr.get());
    if (!cultistAI)
        return;

    switch (cultistAI->bb.ai_state)
    {
    case AIState::Patrol:
    {
        NAVMESH* nav = GetNavMesh(session.room_id);
        if (!nav) 
            return;

        Vec3 cur{
            session.cultist_state.PositionX,
            session.cultist_state.PositionY,
            session.cultist_state.PositionZ
        };

        // 목적지 없으면 새로 생성
        if (!cultistAI->bb.has_patrol_target)
        {
            int curTri = nav->FindContainingTriangle(cur);
            if (curTri < 0)
                return;

            // 랜덤 이웃 삼각형 선택
            int randomTri = nav->GetRandomTriangle(curTri, 10);
            if (randomTri < 0)
                return;

            Vec3 randomPoint = nav->GetTriCenter(randomTri);

            cultistAI->bb.patrol_target = randomPoint;
            cultistAI->bb.has_patrol_target = true;
            cultistAI->bb.stuck_ticks = 0;
            cultistAI->bb.last_dist_to_target = FLT_MAX;
            cultistAI->bb.path.clear();
        }

        float dist = Dist(cur, cultistAI->bb.patrol_target);

        // 도착했으면 새 목적지 만들기
        if (dist < CHASE_STOP_RANGE)
        {
            cultistAI->bb.has_patrol_target = false;
            cultistAI->bb.path.clear();
            return;
        }

        if (dist > cultistAI->bb.last_dist_to_target - STUCK_RANGE)
        {
            cultistAI->bb.stuck_ticks++;
        }
        else
        {
            cultistAI->bb.stuck_ticks = 0;
        }

        cultistAI->bb.last_dist_to_target = dist;

        if (cultistAI->bb.stuck_ticks > 60)
        {
            cultistAI->bb.has_patrol_target = false;
            cultistAI->bb.path.clear();
            return;
        }

        MoveAlongPath(session, cultistAI->bb.patrol_target, dt);
        break;
    }
    case AIState::Chase:
    {
        int target_id = cultistAI->bb.target_id;
        if (target_id < 0)
        {
            cultistAI->bb.path.clear();
            StopMovement(session);
            return;
        }

        auto it = g_users.find(target_id);
        if (it == g_users.end())
        {
            cultistAI->bb.target_id = -1;
            cultistAI->bb.ai_state = AIState::Patrol;
            cultistAI->bb.path.clear();
            break;
        }

        auto target = it->second;
        if (target->cultist_state.CurrentHealth <= 50.f &&
            !session.cultist_state.ABP_DoHeal &&
            !target->cultist_state.ABP_GetHeal)
        {
            session.heal_partner = target_id;
            cultistAI->bb.ai_state = AIState::Heal;
            cultistAI->bb.path.clear();
            return;
        }

        Vec3 selfPos{
           session.cultist_state.PositionX,
           session.cultist_state.PositionY,
           session.cultist_state.PositionZ
        };

        Vec3 targetPos{
            target->cultist_state.PositionX,
            target->cultist_state.PositionY,
            target->cultist_state.PositionZ
        };

        float dist = Dist(selfPos, targetPos);
        if (dist <= CHASE_STOP_RANGE)
        {
            cultistAI->bb.path.clear();
            cultistAI->bb.has_patrol_target = false;
            StopMovement(session);
            return;
        }

        MoveAlongPath(session, targetPos, dt);
        break;
    }
    case AIState::Runaway:
    {
        MAP* map = GetMap(session.room_id);
        if (!map)
            return;
        NAVMESH* nav = GetNavMesh(session.room_id);
        if (!nav) 
            return;
        // EQS Score =
        //    0.30 * 거리 점수
        //    + 0.20 * 방향 점수
        //    + 0.15 * 가시성 점수
        //    + 0.35 * NavMesh 경로 점수
        Vec3 selfPos{
        session.cultist_state.PositionX,
        session.cultist_state.PositionY,
        session.cultist_state.PositionZ
        };

        if (cultistAI->bb.has_runaway_target && cultistAI->bb.runaway_ticks < 30 &&
            Dist(selfPos, cultistAI->bb.runaway_target) > ARRIVE_RANGE)
        {
            cultistAI->bb.runaway_ticks++;
            MoveAlongPath(session, cultistAI->bb.runaway_target, dt);
            break;
        }

        int police_id = cultistAI->bb.target_id;
        if (police_id < 0)
            return;

        auto it = g_users.find(police_id);
        if (it == g_users.end())
            return;

        auto police = it->second;
        Vec3 policePos{
            police->police_state.PositionX,
            police->police_state.PositionY,
            police->police_state.PositionZ
        };

        // 후보 생성 (원형 샘플)
        Vec3 bestPos = selfPos;
        float bestScore = -FLT_MAX;
        
        static std::default_random_engine dre{ std::random_device()() };
        std::uniform_real_distribution<float> angleDist(0.f, 2.f * PI);

        int selfTri = nav->FindContainingTriangle(selfPos);
        if (selfTri < 0) {
            std::cout << "if (selfTri < 0)" << std::endl;
            return;
        }

        std::vector<RunawayCandidate> candidates;
        candidates.reserve(sampleCount);

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

            // 거리 점수
            float distPolice = Dist(candidate, policePos);
            float distScore = std::min(distPolice / (sampleRadius * 2.f), 1.f);
            score += distScore * 1000.f * 0.30f;

            // 방향 점수 (반대 방향 선호)
            Vec3 fleeDir{
                selfPos.x - policePos.x,
                selfPos.y - policePos.y,
                0.f
            };
            Vec3 moveDir{
                candidate.x - selfPos.x,
                candidate.y - selfPos.y,
                0.f
            };

            float len1 = std::sqrt(fleeDir.x * fleeDir.x + fleeDir.y * fleeDir.y);
            float len2 = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y);

            if (len1 > 1e-3f && len2 > 1e-3f)
            {
                fleeDir.x /= len1; fleeDir.y /= len1;
                moveDir.x /= len2; moveDir.y /= len2;

                float dot = fleeDir.x * moveDir.x + fleeDir.y * moveDir.y;
                score += ((dot + 1.f) * 0.5f) * 1000.f * 0.20f;
            }

            // 가시성 점수 (벽 뒤 선호)
            Ray ray;
            ray.start = policePos;

            Vec3 d{
                candidate.x - policePos.x,
                candidate.y - policePos.y,
                0.f
            };

            float len = std::sqrt(d.x * d.x + d.y * d.y);
            if (len > 1e-3f)
            {
                ray.dir.x = d.x / len;
                ray.dir.y = d.y / len;
                ray.dir.z = 0.f;

                float hitDist;
                int hitTri;

                if (map && map->LineTrace(ray, len, hitDist, hitTri)) {
                    score += 500.f * 0.15f; // 벽 뒤면 보너스
                }
            }

            if (nav->triComponentId[selfTri] != nav->triComponentId[tri])
            {
                continue;
            }
            candidates.push_back({ candidate, score, tri });
        }

        std::sort(candidates.begin(), candidates.end());
        const int pathTestCount = std::min(3, static_cast<int>(candidates.size()));

        for (int i = 0; i < pathTestCount; ++i)
        {
            std::vector<int> triPath;
            if (!nav->FindTriPath(selfPos, candidates[i].pos, triPath))
                continue;

            // NavMesh 경로 점수
            float pathScore = std::min(static_cast<float>(triPath.size()) / 50.f, 1.f);
            float finalScore = candidates[i].score + pathScore * 1000.f * 0.35f;

            if (finalScore > bestScore)
            {
                bestScore = finalScore;
                bestPos = candidates[i].pos;
            }
        }

        if (!cultistAI->bb.has_runaway_target)
        {
            cultistAI->bb.runaway_target = bestPos;
            cultistAI->bb.has_runaway_target = true;
            cultistAI->bb.runaway_ticks = 0;
            cultistAI->bb.path.clear();
        }
        else
        {
            cultistAI->bb.runaway_ticks++;
            if (cultistAI->bb.runaway_ticks >= 30 &&
                Dist(cultistAI->bb.runaway_target, bestPos) > 400.f)
            {
                cultistAI->bb.runaway_target = bestPos;
                cultistAI->bb.runaway_ticks = 0;
                cultistAI->bb.path.clear();
            }
        }

        MoveAlongPath(session, cultistAI->bb.runaway_target, dt);
        break;
    }
    case AIState::Heal:
    {
        int target_id = session.heal_partner;
        if (target_id < 0)
            return;

        auto it = g_users.find(target_id);
        if (it == g_users.end())
            return;

        if (session.cultist_state.ABP_DoHeal || session.cultist_state.ABP_GetHeal)
        {
            StopMovement(session);
            return;
        }

        auto target = it->second;

        auto moveOpt = GetMovePoint(session.id, target_id);
        if (!moveOpt)
            return;

        auto [moveLoc, moveRot] = *moveOpt;

        Vec3 selfPos{
        session.cultist_state.PositionX,
        session.cultist_state.PositionY,
        session.cultist_state.PositionZ
        };

        Vec3 healMovePos{
            static_cast<float>(moveLoc.x),
            static_cast<float>(moveLoc.y),
            static_cast<float>(moveLoc.z)
        };

        float moveDist = Dist(selfPos, healMovePos);

        if (moveDist > 30.f)   // 충분히 가까워 지기 전까지 이동만
        {
            MoveAlongPath(session, healMovePos, dt);
            return;
        }

        const double rad = moveRot.yaw * PI / 180.0;
        const double dirX = std::cos(rad);
        const double dirY = std::sin(rad);
        FVector targetPos{
            moveLoc.x + dirX * (HEAL_GAP * 0.5),
            moveLoc.y + dirY * (HEAL_GAP * 0.5),
            moveLoc.z
        };
        double yaw = std::fmod(moveRot.yaw + 180.0, 360.0);
        if (yaw < 0.0) {
            yaw += 360.0;
        }

        MovePacket pkt;
        pkt.header = doHealHeader;
        pkt.size = sizeof(MovePacket);
        pkt.SpawnLoc = targetPos;
        pkt.SpawnRot.yaw = yaw;
        pkt.isHealer = false;
        target->heal_partner = session.id;
        target->do_send_packet(&pkt);

        // Heal 시작
        TIMER_EVENT ev;
        ev.wakeup_time = std::chrono::system_clock::now() + 1s;
        ev.c_id = session.id;
        ev.target_id = target_id;
        ev.event_id = EV_HEAL;
        timer_queue.push(ev);

        session.cultist_state.ABP_DoHeal = 1;
        session.heal_partner = target_id;
        cultistAI->bb.ai_state = AIState::Heal;
        cultistAI->bb.path.clear();
        return;
        break;
    }
    case AIState::Ritual:
    {
        if (cultistAI->bb.ritual_id < 0)
        {
            cultistAI->bb.ai_state = AIState::Patrol;
            break;
        }

        Altar& altar = g_altars[session.room_id][cultistAI->bb.ritual_id];
        Vec3 altarPos{
            static_cast<float>(altar.loc.x),
            static_cast<float>(altar.loc.y),
            static_cast<float>(altar.loc.z)
        };

        // 아직 멀면 이동
        MoveAlongPath(session, altarPos, dt);
        Vec3 selfPos{
            session.cultist_state.PositionX,
            session.cultist_state.PositionY,
            session.cultist_state.PositionZ
        };
        float dist = Dist(selfPos, altarPos);
        if (!cultistAI->bb.path.empty())
        {
            std::cout << "if (!ai.path.empty()))\n";
            break;
        }

        cultistAI->bb.has_patrol_target = false;
        StopMovement(session);
        cultistAI->bb.path.clear();

        // 도착, Ritual 시작
        if (!altar.isActivated)
        {
            altar.isActivated = true;
            altar.time = std::chrono::system_clock::now();
            std::cout << "[AI RitualStart] ai=" << session.id
                << " altar=" << cultistAI->bb.ritual_id << "\n";
            break;
        }
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - altar.time).count();

        if (elapsed > 0)
        {
            int add = static_cast<int>(elapsed / 100); // 0.1초당 1%
            if (add > 0)
            {
                altar.gauge = std::min(100, altar.gauge + add);
                altar.time = now;
            }
        }

        if (altar.gauge >= 100)
        {
            altar.gauge = 100;
            altar.isActivated = false;

            std::cout << "[AI Ritual Complete] ai=" << session.id
                << " altar=" << cultistAI->bb.ritual_id << "\n";

            cultistAI->bb.ritual_id = -1;
            cultistAI->bb.ai_state = AIState::Patrol;
        }
        break;
    }
    default:
        break;
    }
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
            if (st.ABP_IsDead || st.ABP_IsStunned || st.ABP_IsHitByAnAttack)
            {
                StopMovement(*session);
                canMove = false;
            }
        
            if (canMove)
            {
                UpdateAIState(*session);
                ExecuteAIState(*session, dt);
            }
            CultistPacket packet{};
            packet.header = cultistHeader;
            packet.size = sizeof(CultistPacket);
            packet.state = session->cultist_state;
            BroadcastCultistAIState(*session, &packet);
        }

        nextTick += std::chrono::duration_cast<clock::duration>(
            std::chrono::duration<float>(fixed_dt));

        std::this_thread::sleep_until(nextTick);
    }
}

template <typename PacketT>
void BroadcastCultistAIState(const SESSION& session, const PacketT* packet)
{
    if (session.role != 100) // Cultist AI만
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

void ApplyBatonHitToAI(SESSION& session, const Vec3& attackerPos)
{
    auto& st = session.cultist_state;
    if (st.ABP_IsDead)
        return;

    st.ABP_IsHitByAnAttack = 1;
    st.ABP_IsPerforming = 0;
    st.ABP_DoHeal = 0;
    st.ABP_GetHeal = 0;

    auto aiPtr = session.ai;
    auto* cultistAI = dynamic_cast<CultistAIController*>(aiPtr.get());
    if (!cultistAI)
        return;

    cultistAI->bb.path.clear();

    Vec3 cur{
        session.cultist_state.PositionX,
        session.cultist_state.PositionY,
        session.cultist_state.PositionZ
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

    session.cultist_state.PositionX += dir.x * pushDist;
    session.cultist_state.PositionY += dir.y * pushDist;
    StopMovement(session);
    session.cultist_state.RotationYaw = std::atan2(dir.y, dir.x) * RAD_TO_DEG;

    st.CurrentHealth -= 100.f;
    if (st.CurrentHealth <= 0.f)
    {
        if (st.ABP_IsStunned)
        {
            st.ABP_IsDead = 1;
        }
        else
        {
            st.ABP_IsStunned = 1;
            st.ABP_TTStun = 1;

            TIMER_EVENT ev;
            ev.c_id = session.id;
            ev.target_id = session.id;
            ev.event_id = EV_STUN;
            ev.wakeup_time = std::chrono::system_clock::now() + 10s; // 스턴 시간

            timer_queue.push(ev);
        }
    }
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

void CultistAIController::Update(float dt)
{

}