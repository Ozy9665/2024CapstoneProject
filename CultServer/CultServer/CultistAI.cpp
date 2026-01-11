#include "CultistAI.h"
#include <chrono>
#include <thread>

extern std::unordered_map<int, SESSION> g_users;
extern std::unordered_set<int> g_cultist_ai_ids;
extern std::array<std::pair<Room, MAPTYPE>, MAX_ROOM> g_rooms;
extern NEVMESH NewmapLandmassMapNevMesh;

static int FindTargetCultist(int room_id, int self_id)
{
    auto& room = g_rooms[room_id];

    for (int pid : room.first.player_ids)
    {
        if (pid == UINT8_MAX || pid == self_id)
            continue;

        auto it = g_users.find(pid);
        if (it == g_users.end())
            continue;

        SESSION& target = it->second;

        // Cultist 플레이어만 추적
        if (target.role == 0 && target.isValidState())
            return pid;
    }
    return -1;
}

static void MoveAlongPath(SESSION& ai, const Vec3& targetPos, float deltaTime)
{
    Vec3 cur{
        ai.cultist_state.PositionX,
        ai.cultist_state.PositionY,
        ai.cultist_state.PositionZ
    };

    int s = NewmapLandmassMapNevMesh.FindNearestTri(cur);
    int e = NewmapLandmassMapNevMesh.FindNearestTri(targetPos);

    if (s < 0 || e < 0)
        return;

    std::vector<Vec3> path;
    if (!NewmapLandmassMapNevMesh.FindPath(s, e, path))
        return;

    if (path.size() < 2)
        return;

    // 다음 목표 노드
    Vec3 next = path[1];

    const float speed = 300.f; // cm/se
    Vec3 dir{
        next.x - cur.x,
        next.y - cur.y,
        next.z - cur.z
    };

    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len < 1e-3f)
        return;

    dir.x /= len;
    dir.y /= len;
    dir.z /= len;

    // 위치 갱신
    ai.cultist_state.PositionX += dir.x * speed * deltaTime;
    ai.cultist_state.PositionY += dir.y * speed * deltaTime;
    ai.cultist_state.PositionZ += dir.z * speed * deltaTime;
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

void BroadcastCultistAIState(const SESSION& ai)
{
    if (ai.role != 100) // Cultist AI만
        return;

    int room_id = ai.room_id;
    if (room_id < 0 || room_id >= MAX_ROOM)
        return;

    CultistPacket pkt{};
    pkt.header = cultistHeader;
    pkt.size = sizeof(CultistPacket);
    pkt.state = ai.cultist_state;

    auto& room = g_rooms[room_id].first;
    for (int pid : room.player_ids)
    {
        if (pid == INT_MAX || pid == ai.id)
            continue;

        auto it = g_users.find(pid);
        if (it == g_users.end())
            continue;

        SESSION& target = it->second;

        if (!target.isValidSocket() || !target.isValidState())
            continue;

        target.do_send_packet(&pkt);
    }
}
