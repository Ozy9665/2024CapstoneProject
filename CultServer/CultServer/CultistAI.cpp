#define NOMINMAX

#include "CultistAI.h"
#include <chrono>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <cmath>
#include <concurrent_priority_queue.h>
#include <random>

using namespace std;
extern std::unordered_map<int, SESSION> g_users;
extern std::unordered_set<int> g_cultist_ai_ids;
extern std::array<std::pair<Room, MAPTYPE>, MAX_ROOM> g_rooms;
extern MAP NewmapLandmassMap;
extern NAVMESH TestNavMesh;
extern concurrency::concurrent_priority_queue<TIMER_EVENT> timer_queue;
extern std::array<std::array<Altar, ALTAR_PER_ROOM>, MAX_ROOM> g_altars;

void AddCutltistAi(int ai_id, uint8_t ai_role, int room_id)
{
    SESSION ai(ai_id, ai_role, room_id);
    g_users.emplace(ai_id, std::move(ai));
    g_cultist_ai_ids.insert(ai_id);

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

    auto it = g_users.find(ai_id);
    if (it != g_users.end())
    {
        BroadcastCultistAIState(it->second, &pkt);
    }
}

static float Dist(const Vec3& a, const Vec3& b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static void StopMovement(SESSION& ai)
{
    ai.cultist_state.VelocityX = 0.f;
    ai.cultist_state.VelocityY = 0.f;
    ai.cultist_state.VelocityZ = 0.f;
    ai.cultist_state.Speed = 0.f;
}

static void MoveAlongPath(SESSION& ai, const Vec3& targetPos, float deltaTime)
{

    Vec3 cur{
        ai.cultist_state.PositionX,
        ai.cultist_state.PositionY,
        ai.cultist_state.PositionZ
    };

    if (Dist(cur, targetPos) <= CHASE_STOP_RANGE)
    {
        StopMovement(ai);
        return;
    }

    float dx = targetPos.x - ai.lastTargetPos.x;
    float dy = targetPos.y - ai.lastTargetPos.y;
    float dist2 = dx * dx + dy * dy;

    if (dist2 > REPATH_DIST * REPATH_DIST)
    {
        // 타겟이 충분히 이동, 경로 무효화
        ai.lastTargetPos = targetPos;
        ai.path.clear();
    }

    if (ai.path.empty())
    {
        std::vector<int> triPath;
        if (!TestNavMesh.FindTriPath(cur, targetPos, triPath))
        {
            std::cout << "TestNavMesh.FindTriPath fail" << "\n";
            StopMovement(ai);
            ai.path.clear();
            return;
        }

        std::vector<std::pair<Vec3, Vec3>> portals;
        TestNavMesh.BuildPortals(triPath, portals);

        if (portals.empty()) {
            StopMovement(ai);
            return;
        }

        std::vector<Vec3> smoothPath;
        if (!TestNavMesh.SmoothPath(cur, targetPos, portals, smoothPath) ||
            smoothPath.size() < 2)
        {
            StopMovement(ai);
            return;
        }
        ai.path = smoothPath;
    }

    if (ai.path.empty())
    {
        StopMovement(ai);
        return;
    }

    // 다음 목표 노드
    Vec3 next;
    if (ai.path.size() >= 2) {
        next = ai.path[1];
    }
    else {
        next = ai.path[0];
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
        // 현재 노드에 도착했다고 판단
        // 이번 tick에서는 이동하지 않음
        ai.path.erase(ai.path.begin());
        StopMovement(ai);
        return;
    }

    if (len < 1e-3f)
    {
        std::cout << "[AI MOVE] direction too small\n";
        StopMovement(ai);
        return;
    }

    dir.x /= len;
    dir.y /= len;
    // dir.z /= len;

    // 위치 갱신
    ai.cultist_state.PositionX += dir.x * speed * deltaTime;
    ai.cultist_state.PositionY += dir.y * speed * deltaTime;
    // ai.cultist_state.PositionZ += dir.z * speed * deltaTime;

    ai.cultist_state.VelocityX = dir.x * speed;
    ai.cultist_state.VelocityY = dir.y * speed;
    ai.cultist_state.VelocityZ = 0.f;
    ai.cultist_state.Speed = std::sqrt(
            ai.cultist_state.VelocityX * ai.cultist_state.VelocityX +
            ai.cultist_state.VelocityY * ai.cultist_state.VelocityY
        );

    // state 회전 갱신
    Vec3 lookDir{
    targetPos.x - cur.x,
    targetPos.y - cur.y,
    0.f
    };

    float lookLen = std::sqrt(lookDir.x * lookDir.x + lookDir.y * lookDir.y);
    if (lookLen > 1e-3f)
    {
        lookDir.x /= lookLen;
        lookDir.y /= lookLen;

        ai.cultist_state.RotationYaw =
            std::atan2(lookDir.y, lookDir.x) * RAD_TO_DEG;
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

        SESSION& target = it->second;
        if (target.room_id != room_id)
            continue;
        if (!target.isValidSocket())
            continue;
        if (target.role != 0)
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

    Vec3 selfPos{
        selfIt->second.cultist_state.PositionX,
        selfIt->second.cultist_state.PositionY,
        selfIt->second.cultist_state.PositionZ
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

        SESSION& target = it->second;

        if (target.role != 1)
            continue;

        if (!target.isValidSocket())
            continue;

        float dx = target.police_state.PositionX - selfPos.x;
        float dy = target.police_state.PositionY - selfPos.y;
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

    Vec3 selfPos{
        selfIt->second.cultist_state.PositionX,
        selfIt->second.cultist_state.PositionY,
        selfIt->second.cultist_state.PositionZ
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

        SESSION& target = it->second;
        if (target.role != 0)
            continue;

        float dx = target.cultist_state.PositionX - selfPos.x;
        float dy = target.cultist_state.PositionY - selfPos.y;
        float dist_sq = dx * dx + dy * dy;

        if (dist_sq < best_dist_sq)
        {
            best_dist_sq = dist_sq;
            best_id = pid;
        }
    }

    return best_id;
}

static bool IsNearAltar(SESSION& ai)
{
    int room_id = ai.room_id;
    if (room_id < 0 || room_id >= MAX_ROOM)
        return false;

    Vec3 selfPos{
        ai.cultist_state.PositionX,
        ai.cultist_state.PositionY,
        ai.cultist_state.PositionZ
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
            ai.ritual_id = i;
            return true;
        }
    }

    ai.ritual_id = -1;
    return false;
}

static void UpdateAIState(SESSION& ai)
{
    // 경찰 발견 Runaway
    int police_id = FindNearbyPolice(ai.room_id, ai.id);
    if (police_id >= 0)
    {
        if (ai.ai_state == AIState::Heal && ai.heal_partner >= 0)
        {
            auto it = g_users.find(ai.heal_partner);
            if (it != g_users.end())
            {
                NoticePacket packet;
                packet.header = endHealHeader;
                packet.size = sizeof(NoticePacket);

                it->second.do_send_packet(&packet);
            }

            // Heal 상태 해제
            ai.cultist_state.ABP_DoHeal = 0;
            ai.cultist_state.ABP_GetHeal = 0;
            ai.heal_partner = -1;
        }
        else if (ai.ai_state == AIState::Ritual)
        {
            Altar& altar = g_altars[ai.room_id][ai.ritual_id];
            altar.isActivated = false;
            ai.ritual_id = -1;
        }
        ai.ai_state = AIState::Runaway;
        ai.target_id = police_id;
        ai.has_patrol_target = false;
        ai.path.clear();
        return;
    }
    // 치료 중
    if (ai.ai_state == AIState::Heal)
    {
        if (!ai.cultist_state.ABP_DoHeal &&
            !ai.cultist_state.ABP_GetHeal)
        {
            ai.ai_state = AIState::Patrol;
            ai.target_id = -1;
            ai.path.clear();
        }
        StopMovement(ai);
        return;
    }
    // 제단 진행 중
    if (ai.ai_state == AIState::Ritual)
    {
        if (ai.ritual_id < 0)
        {
            ai.ai_state = AIState::Patrol;
            return;
        }
        return;
    }

    // 제단 발견 Ritual
    if (IsNearAltar(ai))
    {
        ai.ai_state = AIState::Ritual;
        ai.has_patrol_target = false;
        ai.path.clear();
        return;
    }

    // 이미 Chase중
    if (ai.ai_state == AIState::Chase && ai.target_id >= 0)
    {
        auto it = g_users.find(ai.target_id);
        if (it == g_users.end())
        {
            ai.ai_state = AIState::Patrol;
            ai.target_id = -1;
            return;
        }
        return;
    }

    // 유저 발견 Chase
    int cultist_id = FindNearbyCultist(ai.room_id, ai.id);
    if (cultist_id >= 0)
    {
        auto it = g_users.find(cultist_id);
        if (it != g_users.end())
        {
            Vec3 selfPos{
               ai.cultist_state.PositionX,
               ai.cultist_state.PositionY,
               ai.cultist_state.PositionZ
            };
            Vec3 targetPos{
                it->second.cultist_state.PositionX,
                it->second.cultist_state.PositionY,
                it->second.cultist_state.PositionZ
            };

            float dist = Dist(selfPos, targetPos);
            if (dist <= CHASE_START_RANGE)
            {
                ai.ai_state = AIState::Chase;
                ai.target_id = cultist_id;
                ai.has_patrol_target = false;
                ai.path.clear();
                return;
            }
        }
    }

    // 기본 상태
    ai.ai_state = AIState::Patrol;
}

static void ExecuteAIState(SESSION& ai, float dt) 
{
    switch (ai.ai_state)
    {
    case AIState::Patrol:
    {
        Vec3 cur{
            ai.cultist_state.PositionX,
            ai.cultist_state.PositionY,
            ai.cultist_state.PositionZ
        };

        // 목적지 없으면 새로 생성
        if (!ai.has_patrol_target)
        {
            int curTri = TestNavMesh.FindContainingTriangle(cur);
            if (curTri < 0)
                return;

            // 랜덤 이웃 삼각형 선택
            int randomTri = TestNavMesh.GetRandomTriangle(curTri, 10);
            if (randomTri < 0)
                return;

            Vec3 randomPoint = TestNavMesh.GetTriCenter(randomTri);

            ai.patrol_target = randomPoint;
            ai.has_patrol_target = true;
            ai.stuck_ticks = 0;
            ai.last_dist_to_target = FLT_MAX;
            ai.path.clear();
        }

        float dist = Dist(cur, ai.patrol_target);

        // 도착했으면 새 목적지 만들기
        if (dist < CHASE_STOP_RANGE)
        {
            ai.has_patrol_target = false;
            ai.path.clear();
            return;
        }

        if (dist > ai.last_dist_to_target - 5.f)
        {
            ai.stuck_ticks++;
        }
        else
        {
            ai.stuck_ticks = 0;
        }

        ai.last_dist_to_target = dist;

        if (ai.stuck_ticks > 60)
        {
            ai.has_patrol_target = false;
            ai.path.clear();
            return;
        }

        MoveAlongPath(ai, ai.patrol_target, dt);
        break;
    }
    case AIState::Chase:
    {
        int target_id = ai.target_id;
        if (target_id < 0)
        {
            ai.path.clear();
            StopMovement(ai);
            return;
        }

        auto it = g_users.find(target_id);
        if (it == g_users.end())
        {
            ai.target_id = -1;
            ai.ai_state = AIState::Patrol;
            ai.path.clear();
            break;
        }

        SESSION& target = it->second;
        if (target.cultist_state.CurrentHealth <= 50.f &&
            !ai.cultist_state.ABP_DoHeal &&
            !target.cultist_state.ABP_GetHeal)
        {
            ai.heal_partner = target_id;
            ai.ai_state = AIState::Heal;
            ai.path.clear();
            return;
        }

        Vec3 selfPos{
           ai.cultist_state.PositionX,
           ai.cultist_state.PositionY,
           ai.cultist_state.PositionZ
        };

        Vec3 targetPos{
            target.cultist_state.PositionX,
            target.cultist_state.PositionY,
            target.cultist_state.PositionZ
        };

        float dist = Dist(selfPos, targetPos);
        if (dist <= CHASE_STOP_RANGE)
        {
            ai.path.clear();
            ai.has_patrol_target = false;
            StopMovement(ai);
            return;
        }

        MoveAlongPath(ai, targetPos, dt);
        break;
    }
    case AIState::Runaway:
    {
        // EQS Score =
        //    0.45 * 거리 점수
        //    + 0.30 * 방향 점수
        //    + 0.15 * 가시성 점수
        //    + 0.10 * NavMesh 경로 점수
        int police_id = ai.target_id;
        if (police_id < 0)
            return;

        auto it = g_users.find(police_id);
        if (it == g_users.end())
            return;

        SESSION& police = it->second;

        Vec3 selfPos{
        ai.cultist_state.PositionX,
        ai.cultist_state.PositionY,
        ai.cultist_state.PositionZ
        };

        Vec3 policePos{
            police.police_state.PositionX,
            police.police_state.PositionY,
            police.police_state.PositionZ
        };

        // 후보 생성 (원형 샘플)
        Vec3 bestPos = selfPos;
        float bestScore = -FLT_MAX;
        
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

            int tri = TestNavMesh.FindContainingTriangle(candidate);
            if (tri < 0)
                continue;

            float score = 0.f;

            // 거리 점수
            float distPolice = Dist(candidate, policePos);
            float distScore = std::min(distPolice / (sampleRadius * 2.f), 1.f);
            score += distScore * 1000.f * 0.45f;

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
                score += ((dot + 1.f) * 0.5f) * 1000.f * 0.30f;
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

                if (NewmapLandmassMap.LineTrace(ray, len, hitDist, hitTri))
                    score += 500.f * 0.15f; // 벽 뒤면 보너스
            }

            // NavMesh 경로 점수
            std::vector<int> triPath;
            if (TestNavMesh.FindTriPath(policePos, candidate, triPath))
            {
                float pathScore = std::min((float)triPath.size() / 50.f, 1.f);
                score += pathScore * 1000.f * 0.10f;
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestPos = candidate;
            }
        }

        MoveAlongPath(ai, bestPos, dt);
        break;
    }
    case AIState::Heal:
    {
        int target_id = ai.heal_partner;
        if (target_id < 0)
            return;

        auto it = g_users.find(target_id);
        if (it == g_users.end())
            return;

        if (ai.cultist_state.ABP_DoHeal || ai.cultist_state.ABP_GetHeal)
        {
            StopMovement(ai);
            return;
        }

        SESSION& target = it->second;

        auto moveOpt = GetMovePoint(ai.id, target_id);
        if (!moveOpt)
            return;

        auto [moveLoc, moveRot] = *moveOpt;

        Vec3 selfPos{
        ai.cultist_state.PositionX,
        ai.cultist_state.PositionY,
        ai.cultist_state.PositionZ
        };

        Vec3 healMovePos{
            static_cast<float>(moveLoc.x),
            static_cast<float>(moveLoc.y),
            static_cast<float>(moveLoc.z)
        };

        float moveDist = Dist(selfPos, healMovePos);

        if (moveDist > 30.f)   // 충분히 가까워 지기 전까지 이동만
        {
            MoveAlongPath(ai, healMovePos, dt);
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
        target.heal_partner = ai.id;
        target.do_send_packet(&pkt);

        // Heal 시작
        TIMER_EVENT ev;
        ev.wakeup_time = std::chrono::system_clock::now() + 1s;
        ev.c_id = ai.id;
        ev.target_id = target_id;
        ev.event_id = EV_HEAL;
        timer_queue.push(ev);

        ai.cultist_state.ABP_DoHeal = 1;
        ai.heal_partner = target_id;
        ai.ai_state = AIState::Heal;
        ai.path.clear();
        return;
        break;
    }
    case AIState::Ritual:
    {
        if (ai.ritual_id < 0)
        {
            ai.ai_state = AIState::Patrol;
            break;
        }

        Vec3 selfPos{
            ai.cultist_state.PositionX,
            ai.cultist_state.PositionY,
            ai.cultist_state.PositionZ
        };

        Altar& altar = g_altars[ai.room_id][ai.ritual_id];
        Vec3 altarPos{
            static_cast<float>(altar.loc.x),
            static_cast<float>(altar.loc.y),
            static_cast<float>(altar.loc.z)
        };

        float dist = Dist(selfPos, altarPos);

        // 아직 멀면 이동
        if (dist >= CHASE_STOP_RANGE)
        {
            MoveAlongPath(ai, altarPos, dt);
            break;
        }

        ai.has_patrol_target = false;
        StopMovement(ai);
        ai.path.clear();

        // 도착, Ritual 시작
        if (!altar.isActivated)
        {
            altar.isActivated = true;
            altar.time = std::chrono::system_clock::now();
            std::cout << "[AI RitualStart] ai=" << ai.id
                << " altar=" << ai.ritual_id << "\n";
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

            std::cout << "[AI Ritual Complete] ai=" << ai.id
                << " altar=" << ai.ritual_id << "\n";

            ai.ritual_id = -1;
            ai.ai_state = AIState::Patrol;
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

            SESSION& ai = it->second;
            if (ai.role != 100)
                continue;

            bool canMove = true;
            auto& st = ai.cultist_state;
            if (st.ABP_IsDead || st.ABP_IsStunned || st.ABP_IsHitByAnAttack)
            {
                StopMovement(ai);
                canMove = false;
            }
        
            if (canMove)
            {
                UpdateAIState(ai);
                ExecuteAIState(ai, dt);
            }
            CultistPacket packet{};
            packet.header = cultistHeader;
            packet.size = sizeof(CultistPacket);
            packet.state = ai.cultist_state;
            BroadcastCultistAIState(ai, &packet);
        }

        nextTick += std::chrono::duration_cast<clock::duration>(
            std::chrono::duration<float>(fixed_dt));

        std::this_thread::sleep_until(nextTick);
    }
}

template <typename PacketT>
void BroadcastCultistAIState(const SESSION& ai, const PacketT* packet)
{
    if (ai.role != 100) // Cultist AI만
        return;

    int room_id = ai.room_id;
    if (room_id < 0 || room_id >= MAX_ROOM)
    {
        return;
    }

    auto& room = g_rooms[room_id].first;

    for (int pid : room.player_ids)
    {
        if (pid == INT_MAX || pid == ai.id)
            continue;

        auto it = g_users.find(pid);
        if (it == g_users.end())
            continue;

        SESSION& target = it->second;

        if (!target.isValidSocket())
            continue;

        target.do_send_packet(reinterpret_cast<void*>(const_cast<PacketT*>(packet)));
    }
}

void ApplyBatonHitToAI(SESSION& ai, const Vec3& attackerPos)
{
    auto& st = ai.cultist_state;
    if (st.ABP_IsDead)
        return;

    st.ABP_IsHitByAnAttack = 1;
    st.ABP_IsPerforming = 0;
    st.ABP_DoHeal = 0;
    st.ABP_GetHeal = 0;

    ai.path.clear();

    Vec3 cur{
        ai.cultist_state.PositionX,
        ai.cultist_state.PositionY,
        ai.cultist_state.PositionZ
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

    ai.cultist_state.PositionX += dir.x * pushDist;
    ai.cultist_state.PositionY += dir.y * pushDist;
    StopMovement(ai);
    ai.cultist_state.RotationYaw = std::atan2(dir.y, dir.x) * RAD_TO_DEG;

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
            ev.c_id = ai.id;
            ev.target_id = ai.id;
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

    const SESSION& healer = itHealer->second;
    const SESSION& target = itTarget->second;

    FVector mid{
         static_cast<double>((healer.cultist_state.PositionX + target.cultist_state.PositionX) * 0.5),
         static_cast<double>((healer.cultist_state.PositionY + target.cultist_state.PositionY) * 0.5),
         static_cast<double>((healer.cultist_state.PositionZ + target.cultist_state.PositionZ) * 0.5)
    };

    const double dx = static_cast<double>(target.cultist_state.PositionX - healer.cultist_state.PositionX);
    const double dy = static_cast<double>(target.cultist_state.PositionY - healer.cultist_state.PositionY);

    double yawHealer = std::atan2(dy, dx) * 180.0 / PI;
    if (yawHealer < 0.0) {
        yawHealer += 360.0;
    }
    FRotator rot{ 0.0, yawHealer, 0.0 };

    return std::make_pair(mid, rot);
}