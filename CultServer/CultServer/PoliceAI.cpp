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
    session->ai->bb.path.clear();
    {
        std::lock_guard<std::mutex> lk(session->s_lock);
        session->ai->bb.ai_state = AIState::Free;
    }
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

            if (!session->ai)
                continue;

            if (session->ai->bb.ai_state == AIState::Free)
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

void AIController::UpdateBlackboard(float dt) 
{
    // BB업데이트
}

void AIController::RunBehaviorTree(float dt) 
{
    // selector 연산 후 함수 실행
}

void AIController::Update(float dt)
{
    UpdateBlackboard(dt);
    RunBehaviorTree(dt);
}