#include "CultistAI.h"
#include <chrono>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <array>

extern std::unordered_map<int, SESSION> g_users;
extern std::unordered_set<int> g_cultist_ai_ids;
extern std::array<std::pair<Room, MAPTYPE>, MAX_ROOM> g_rooms;
extern NEVMESH NewmapLandmassMapNevMesh;
extern MAP NewmapLandmassMap;
extern MAP TestNavMesh;

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

static int FindTargetCultist(int room_id, int self_id)
{
    if (room_id < 0 || room_id >= MAX_ROOM)
        return -1;

    const auto& room = g_rooms[room_id];
    for (int pid : room.first.player_ids)
    {
        if (pid == INT_MAX || pid == self_id)
            continue;
        if (g_cultist_ai_ids.count(pid))
            continue;

        auto it = g_users.find(pid);
        if (it == g_users.end())
            continue;

        SESSION& target = it->second;
        if (target.role != 0 || !target.isValidState())
            continue;

        return pid;
    }
}

static void MoveAlongPath(SESSION& ai, const Vec3& targetPos, float deltaTime)
{
    Vec3 cur{
        ai.cultist_state.PositionX,
        ai.cultist_state.PositionY,
        ai.cultist_state.PositionZ
    };

    if (ai.path.empty() || Dist(cur, targetPos) > REPATH_DIST)
    {
        TestNavMesh.FindPath(cur, targetPos, ai.path);
    }

    if (ai.path.size() < 2)
    {
        std::cout << "[AI MOVE] path too short\n";
        return;
    }

    // 다음 목표 노드
    Vec3 next = ai.path[1];

    Vec3 dir{
        next.x - cur.x,
        next.y - cur.y,
        0.f
        // next.z - cur.z
    };
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    // float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    const float arriveDist = 30.f; // cm

    if (len < arriveDist)
    {
        // 현재 노드에 도착했다고 판단
        // 이번 tick에서는 이동하지 않음
        ai.path.erase(ai.path.begin());

        ai.cultist_state.VelocityX = 0.f;
        ai.cultist_state.VelocityY = 0.f;
        ai.cultist_state.VelocityZ = 0.f;
        ai.cultist_state.Speed = 0.f;

        return;
    }

    if (len < 1e-3f)
    {
        std::cout << "[AI MOVE] direction too small\n";
        return;
    }

    dir.x /= len;
    dir.y /= len;
    // dir.z /= len;

    // 위치 갱신
    const float speed = 1200.f; // cm/se    // 600은 느림
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
    constexpr float RAD_TO_DEG = 180.f / PI;
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

        constexpr float RAD_TO_DEG = 180.f / 3.1415926535f;
        ai.cultist_state.RotationYaw =
            std::atan2(lookDir.y, lookDir.x) * RAD_TO_DEG;
    }

    std::cout << "[AI MOVE] newPos=("
        << ai.cultist_state.PositionX << ","
        << ai.cultist_state.PositionY << ","
        //<< ai.cultist_state.PositionZ 
        << ")\n";
}

void CultistAIWorkerLoop()
{
    constexpr float dt = 1.0f / 60.0f;

    while (true)
    {
        auto tickStart = std::chrono::steady_clock::now();
        for (int ai_id : g_cultist_ai_ids)
        {
            auto it = g_users.find(ai_id);
            if (it == g_users.end())
                continue;

            SESSION& ai = it->second;
            if (ai.role != 100) // Cultist AI만
                continue;

            int room_id = ai.room_id;
            int target_id = FindTargetCultist(room_id, ai_id);
            if (target_id < 0)
                continue;

            SESSION& target = g_users[target_id];
            Vec3 targetPos{
                target.cultist_state.PositionX,
                target.cultist_state.PositionY,
                target.cultist_state.PositionZ
            };

            MoveAlongPath(ai, targetPos, dt);
            CultistPacket packet{};
            packet.header = cultistHeader;
            packet.size = sizeof(CultistPacket);
            packet.state = ai.cultist_state;
            BroadcastCultistAIState(ai, &packet);
        }

        // 60fps 유지
        auto tickEnd = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = tickEnd - tickStart;
        float sleepTime = dt - elapsed.count();

        if (sleepTime > 0.f)
            std::this_thread::sleep_for(
                std::chrono::duration<float>(sleepTime));
    }
}

template <typename PacketT>
void BroadcastCultistAIState(const SESSION& ai, const PacketT* packet)
{
    if (ai.role != 100) // Cultist AI만
        return;

    int room_id = ai.room_id;
    if (room_id < 0 || room_id >= MAX_ROOM)
        return;

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