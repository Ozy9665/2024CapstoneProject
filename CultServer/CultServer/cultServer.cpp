#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NOMINMAX

#include <iostream>
#include <array>
#include <unordered_map>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <string>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <unordered_set>
#include <optional>
#include <numeric>
#include <chrono>
#include <concurrent_priority_queue.h>
#include <algorithm>
#include "Protocol.h"
#include "error.h"
#include "PoliceAI.h"
#include "db.h"
#include "map.h"

#pragma comment(lib, "MSWSock.lib")
#pragma comment (lib, "WS2_32.LIB")
using namespace std;

SOCKET g_s_socket, g_c_socket;
EXP_OVER g_a_over;
HANDLE g_h_iocp = nullptr;

std::atomic<int> client_id = 0;
std::unordered_map<int, SESSION> g_users;
std::array<Room, MAX_ROOM> g_rooms;

std::array<std::array<Altar, 5>, MAX_ROOM> g_altars{};

void InitializeAltars(int room_num) {
	if (room_num < 0 || room_num >= static_cast<int>(g_altars.size())) {
		return;
	}

	std::array<int, 5> order{ 0, 1, 2, 3, 4 };
	for (int i = 0; i < 4; ++i) {
		int j = i + rand() % (5 - i);
		std::swap(order[i], order[j]);
	}

	for (int i = 0; i < 5; ++i) {
		g_altars[room_num][i].loc = kPredefinedLocations[order[i]];
		g_altars[room_num][i].isActivated = false;
		g_altars[room_num][i].id = i;
		g_altars[room_num][i].gauge = 0;
	}
}

// db event 큐
enum DB_EVENT { EV_LOGIN, EV_EXIST, EV_SIGNUP };
struct DBTask {
	int c_id;
	DB_EVENT event_id;
	std::string id;
	std::string pw;
};

std::mutex g_db_mtx;
std::condition_variable g_db_cv;
std::queue<DBTask> g_db_q;

std::mutex g_logged_mtx;
std::unordered_set<std::string> g_logged_in_ids;

void DBWorkerLoop() {
	while (true)
	{
		DBTask task;
		{
			std::unique_lock<std::mutex> lk(g_db_mtx);
			g_db_cv.wait(lk, [] { return !g_db_q.empty(); });
			task = std::move(g_db_q.front());
			g_db_q.pop();
		}

		EXP_OVER* over = new EXP_OVER();
		BoolPacket pkt{};
		pkt.size = sizeof(BoolPacket);

		switch (task.event_id)
		{
		case EV_LOGIN:
		{
			pkt.header = loginHeader;

			// 1) 이미 로그인된 ID면 DB 스킵
			bool already = false;
			{
				std::lock_guard<std::mutex> lk(g_logged_mtx);
				if (g_logged_in_ids.count(task.id)) already = true;
			}

			if (already) {
				pkt.result = false;
				pkt.reason = 4;
			}
			else {
				const int code = logIn(task.id, task.pw);
				if (code == 0) {
					pkt.result = true;
					pkt.reason = 0;
					std::strncpy(over->account_id, task.id.c_str(), sizeof(over->account_id) - 1);
					std::lock_guard<std::mutex> lk(g_logged_mtx);
					g_logged_in_ids.insert(task.id);
				}
				else {
					pkt.result = false;
					pkt.reason = static_cast<uint8_t>(code);
				}
			}

			over->comp_type = OP_LOGIN;
			break;
		}
		case EV_EXIST:
		{
			pkt.header = idExistHeader;
			int code = checkValidID(task.id);
			pkt.result = (code == 0);
			pkt.reason = static_cast<uint8_t>(code);
			over->comp_type = OP_ID_EXIST;
			break;
		}
		case EV_SIGNUP:
		{
			pkt.header = signUpHeader;
			int code = createNewID(task.id, task.pw);
			pkt.result = (code == 0);
			pkt.reason = static_cast<uint8_t>(code);
			over->comp_type = OP_SIGNUP;
			break;
		}
		default:
			pkt.header = 0;
			pkt.result = false;
			pkt.reason = 0xFF;
			over->comp_type = OP_LOGIN;
			break;
		}

		std::memcpy(over->send_buffer, &pkt, sizeof(pkt));
		PostQueuedCompletionStatus(g_h_iocp, pkt.size, static_cast<ULONG_PTR>(task.c_id), &over->over);
	}
}

// room thread
enum ROOM_EVENT { RM_REQ, RM_ENTER, RM_GAMESTART, RM_RITUAL, RM_DISCONNECT, RM_QUIT };
struct RoomTask {
	int c_id;
	ROOM_EVENT type;
	int role;
	int room_id;
};

std::mutex g_room_mtx;
std::condition_variable g_room_cv;
std::queue<RoomTask> g_room_q;

void RoomWorkerLoop() {
	while (true)
	{
		RoomTask task;
		{
			std::unique_lock<std::mutex> lk(g_room_mtx);
			g_room_cv.wait(lk, [] { return !g_room_q.empty(); });
			task = std::move(g_room_q.front());
			g_room_q.pop();
		}

		switch (task.type)
		{
		case RM_REQ:
		{
			if (!g_users.count(task.c_id)) continue;
			int role = task.role;
			g_users[task.c_id].setRole(role);

			RoomsPakcet pkt{};
			pkt.header = requestHeader;
			pkt.size = sizeof(RoomsPakcet);

			int inserted = 0;
			for (const Room& r : g_rooms) {
				if (inserted >= 10) break;
				if (!r.isIngame) {
					if ((role == 1 && r.police < MAX_POLICE_PER_ROOM) ||
						(role == 0 && r.cultist < MAX_CULTIST_PER_ROOM))
					{
						pkt.rooms[inserted++] = r;
					}
				}
			}

			EXP_OVER* eo = new EXP_OVER();
			std::memcpy(eo->send_buffer, &pkt, sizeof(pkt));
			eo->comp_type = OP_ROOM_REQ;
			PostQueuedCompletionStatus(g_h_iocp, pkt.size, static_cast<ULONG_PTR>(task.c_id), &eo->over);
			continue;
		}
		case RM_ENTER:
		{
			if (!g_users.count(task.c_id)) continue;

			auto& user = g_users[task.c_id];
			NoticePacket pkt{};
			pkt.size = sizeof(NoticePacket);

			int room_id = task.room_id;
			if (user.room_id >= 0 && room_id < 0 || room_id >= MAX_ROOM) {
				pkt.header = leaveHeader;
			}
			else {
				if (0 == task.role) {
					pkt.header = (g_rooms[room_id].cultist >= MAX_CULTIST_PER_ROOM) ? leaveHeader : enterHeader;
					if (pkt.header == enterHeader) 
						user.room_id = room_id;
				}
				else if (1 == task.role) {
					pkt.header = (g_rooms[room_id].police >= MAX_POLICE_PER_ROOM) ? leaveHeader : enterHeader;
					if (pkt.header == enterHeader) 
						user.room_id = room_id;
				}
				else {
					pkt.header = leaveHeader;
				}
			}

			EXP_OVER* eo = new EXP_OVER();
			std::memcpy(eo->send_buffer, &pkt, sizeof(pkt));
			eo->comp_type = OP_ROOM_ENTER;
			PostQueuedCompletionStatus(g_h_iocp, pkt.size, static_cast<ULONG_PTR>(task.c_id), &eo->over);
			continue;
		}
		case RM_GAMESTART:
		{
			if (!g_users.count(task.c_id)) continue;
			int room_id = task.room_id;
			if (room_id < 0 || room_id >= MAX_ROOM) continue;

			auto& me = g_users[task.c_id];

			// 카운터, ingame, player_ids 갱신
			if (1 == task.role) g_rooms[room_id].police++;
			else if (0 == task.role) g_rooms[room_id].cultist++;
			if (g_rooms[room_id].cultist >= MAX_CULTIST_PER_ROOM &&
				g_rooms[room_id].police >= MAX_POLICE_PER_ROOM) {
				g_rooms[room_id].isIngame = true;
			}
			me.room_id = room_id;
			for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i) {
				if (g_rooms[room_id].player_ids[i] == UINT8_MAX) {
					g_rooms[room_id].player_ids[i] = static_cast<uint8_t>(task.c_id);
					break;
				}
			}

			// 본인에게
			{
				IdRolePacket pkt{};
				pkt.header = connectionHeader;
				pkt.size = sizeof(IdRolePacket);
				pkt.id = static_cast<uint8_t>(task.c_id);
				pkt.role = static_cast<uint8_t>(task.role);

				EXP_OVER* eo = new EXP_OVER();
				std::memcpy(eo->send_buffer, &pkt, sizeof(pkt));
				eo->comp_type = OP_ROOM_GAMESTART;
				PostQueuedCompletionStatus(g_h_iocp, pkt.size, static_cast<ULONG_PTR>(task.c_id), &eo->over);
			}

			// 서로 교환
			for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i) {
				uint8_t other = g_rooms[room_id].player_ids[i];
				if (other == UINT8_MAX || other == task.c_id) continue;
				if (!g_users.count(other)) continue;

				// 다른 유저에게 나
				{
					IdRolePacket pkt{};
					pkt.header = connectionHeader;
					pkt.size = sizeof(IdRolePacket);
					pkt.id = static_cast<uint8_t>(task.c_id);
					pkt.role = static_cast<uint8_t>(task.role);

					EXP_OVER* eo = new EXP_OVER();
					std::memcpy(eo->send_buffer, &pkt, sizeof(pkt));
					eo->comp_type = OP_ROOM_GAMESTART;
					PostQueuedCompletionStatus(g_h_iocp, pkt.size, static_cast<ULONG_PTR>(other), &eo->over);
				}
				// 나에게 다른 유저
				{
					IdRolePacket pkt{};
					pkt.header = connectionHeader;
					pkt.size = sizeof(IdRolePacket);
					pkt.id = other;
					pkt.role = static_cast<uint8_t>(g_users[other].getRole());

					EXP_OVER* eo = new EXP_OVER();
					std::memcpy(eo->send_buffer, &pkt, sizeof(pkt));
					eo->comp_type = OP_ROOM_GAMESTART;
					PostQueuedCompletionStatus(g_h_iocp, pkt.size, static_cast<ULONG_PTR>(task.c_id), &eo->over);
				}
			}
			continue;
		}
		case RM_RITUAL:
		{
			if (!g_users.count(task.c_id)) continue;
			int room_id = g_users[task.c_id].room_id;
			if (room_id < 0 || room_id >= MAX_ROOM) continue;

			RitualPacket pkt{};
			pkt.header = ritualHeader;
			pkt.size = sizeof(RitualPacket);

			const auto& altars = g_altars[room_id];
			pkt.Loc1 = altars[0].loc;
			pkt.Loc2 = altars[1].loc;
			pkt.Loc3 = altars[2].loc;

			EXP_OVER* eo = new EXP_OVER();
			std::memcpy(eo->send_buffer, &pkt, sizeof(pkt));
			eo->comp_type = OP_ROOM_RITUAL;
			PostQueuedCompletionStatus(g_h_iocp, pkt.size, static_cast<ULONG_PTR>(task.c_id), &eo->over);
			continue;
		}
		case RM_DISCONNECT:
		{
			if (!g_users.count(task.c_id)) continue;
			int room_id = task.room_id;
			if (room_id < 0 || room_id >= MAX_ROOM) break;

			if (0 == task.role && g_rooms[room_id].cultist >= 0) {
				g_rooms[room_id].cultist--;
			}
			else if (1 == task.role && g_rooms[room_id].police >= 0) {
				g_rooms[room_id].police--;
			}

			for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i) {
				if (g_rooms[room_id].player_ids[i] == static_cast<uint8_t>(task.c_id)) {
					g_rooms[room_id].player_ids[i] = UINT8_MAX;
					break;
				}
			}

			IdOnlyPacket pkt;
			pkt.header = DisconnectionHeader;
			pkt.size = sizeof(IdOnlyPacket);
			pkt.id = static_cast<uint8_t>(task.c_id);
			for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i) {
				uint8_t other = g_rooms[room_id].player_ids[i];
				if (other == UINT8_MAX || !g_users.count(other) || !g_users[other].isValidSocket()) {
					continue;
				}
				EXP_OVER* eo = new EXP_OVER();
				std::memcpy(eo->send_buffer, &pkt, sizeof(pkt));
				eo->comp_type = OP_ROOM_DISCONNECT;
				PostQueuedCompletionStatus(g_h_iocp, pkt.size, static_cast<ULONG_PTR>(other), &eo->over);
			}
			continue;
		}
		case RM_QUIT:
		{
			if (!g_users.count(task.c_id)) continue;
			int room_id = task.room_id;
			if (room_id < 0 || room_id >= MAX_ROOM) break;

			if (0 == task.role && g_rooms[room_id].cultist >= 0) {
				g_rooms[room_id].cultist--;
			}
			else if (1 == task.role && g_rooms[room_id].police >= 0) {
				g_rooms[room_id].police--;
			}

			for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i) {
				if (g_rooms[room_id].player_ids[i] == static_cast<uint8_t>(task.c_id)) {
					g_rooms[room_id].player_ids[i] = UINT8_MAX;
					break;
				}
			}
		}
		default:
			break;
		}
	}
}
	/* 임시 */
	/* 
	case leaveHeader: 
	{
		// 1. 방에서 나갔으니까 page가 0인 RoomdataPakcet 전송.
		// 2. room.player_ids에서 해당 id 제거 (=-1)
		// 3. 역할에 따라 room.police 또는 cultist--
		// 4. isIngame = false;
		break;
	}
	case readyHeader:
	{
		// 1. player 상태를 ready로 변경
		// 2. InRoomPacket으로 방 상태 업데이트를 방 전원에게 broadcast
		g_users[c_id].setState(ST_READY);
		break;
	}
	*/
 
// heal timer queue
enum EVENT_TYPE { EV_HEAL };
struct TIMER_EVENT {
	int c_id;
	int target_id;
	std::chrono::system_clock::time_point wakeup_time;
	EVENT_TYPE event_id;

	constexpr bool operator < (const TIMER_EVENT& Left) const
	{
		return (wakeup_time > Left.wakeup_time);
	}
};
concurrency::concurrent_priority_queue<TIMER_EVENT> timer_queue;

void HealTimerLoop() {
	while (true) {
		TIMER_EVENT ev;
		auto current_time = std::chrono::system_clock::now();
		if (true == timer_queue.try_pop(ev)) {
			if (!g_users[ev.c_id].cultist_state.ABP_DoHeal ||
				!g_users[ev.target_id].cultist_state.ABP_GetHeal) {
				continue;
			}
			if (ev.wakeup_time > current_time) {
				std::this_thread::sleep_until(ev.wakeup_time);
			}
			switch (ev.event_id) {
			case EV_HEAL:
				g_users[ev.c_id].heal_gage += 1;
				if (g_users[ev.c_id].heal_gage < 10) {
					ev.wakeup_time = std::chrono::system_clock::now() + 1s;
					ev.event_id = EV_HEAL;
					timer_queue.push(ev);
				}
				else if (g_users[ev.c_id].heal_gage >= 10) {
					// 락 걸기
					if (g_users[ev.c_id].cultist_state.CurrentHealth + 50 > 100)
						g_users[ev.c_id].cultist_state.CurrentHealth = 100;
					else
						g_users[ev.c_id].cultist_state.CurrentHealth += 10;
					g_users[ev.c_id].heal_gage = 0;
					BoolPacket packet;
					packet.header = endHealHeader;
					packet.result = true;
					packet.size = sizeof(BoolPacket);
					g_users[ev.c_id].do_send_packet(&packet);
					g_users[ev.target_id].do_send_packet(&packet);
				}
				break;
			}
		}
		else
		{
			// 큐가 비어 있으면 잠깐 휴식
			std::this_thread::sleep_for(10ms);
		}
	}
}

// map
AABB g_mapAABB;								// 맵 전체 AABB
std::vector<MapVertex> g_mapVertices;		// OBJ 정점(v)
std::vector<MapTriangle> g_mapTriangles;	// OBJ 인덱스(f)
std::vector<MapTri> g_mapTris;				// 실제 삼각형(좌표)
std::vector<AABB> g_triAABBs;				// 삼각형별 AABB
SpatialGrid g_grid;              // 공간 분할 Grid
float cellSize = 300.0f;         // 셀 크기 (UE 기준 200~500 권장)

void disconnect(int);
void CommandWorker()
{
	while (true)
	{
		std::string line;
		std::getline(std::cin, line);

		if (line.empty())
			continue;

		std::istringstream iss(line);
		std::string cmd;
		iss >> cmd;

		if (cmd == "disconnect")
		{
			int target_id;
			if (iss >> target_id)
			{
				if (g_users.count(target_id))
				{
					std::cout << "[Command] Forcing disconnect of ID: " << target_id << "\n";
					disconnect(target_id);
				}
				else
				{
					std::cout << "[Command] No such user ID: " << target_id << "\n";
				}
			}
			else
			{
				std::cout << "[Command] Usage: disconnect <id>\n";
			}
		}
		if (cmd == "add_ai")
		{
			int ai_id = client_id++;
			std::cout << "[Command] New AI added. ID: " << ai_id << "\n";
		}
		else if (cmd == "exit")
		{
			std::cout << "[Command] Exiting server.\n";
			exit(0);
		}
		else
		{
			std::cout << "[Command] Unknown command: " << cmd << "\n";
		}
	}
}

void disconnect(int c_id)
{
	if (!g_users.count(c_id))
		return;
	std::cout << "socket disconnect " << c_id << std::endl;

	auto& user = g_users[c_id];
	if(user.role != -1 && user.room_id != -1)
	{
		RoomTask task;
		task.c_id = c_id;
		task.type = RM_DISCONNECT;
		task.role = user.role;
		task.room_id = user.room_id;

		{
			std::lock_guard<std::mutex> lk(g_room_mtx);
			g_room_q.push(std::move(task));
			g_room_cv.notify_one();
		}
	}

	{
		std::lock_guard<std::mutex> lk(g_logged_mtx);
		if (!user.account_id.empty())
			g_logged_in_ids.erase(user.account_id);
	}
	closesocket(g_users[c_id].c_socket);
	g_users.erase(c_id);
}

float distanceSq(const SESSION& a, const SESSION& b) {
	float ax, ay, az;
	float bx, by, bz;

	// a의 위치
	if (a.role == 0) { // cultist
		ax = a.cultist_state.PositionX;
		ay = a.cultist_state.PositionY;
		az = a.cultist_state.PositionZ;
	}
	else {           // police
		ax = a.police_state.PositionX;
		ay = a.police_state.PositionY;
		az = a.police_state.PositionZ;
	}

	// b의 위치
	if (b.role == 0) {
		bx = b.cultist_state.PositionX;
		by = b.cultist_state.PositionY;
		bz = b.cultist_state.PositionZ;
	}
	else {
		bx = b.police_state.PositionX;
		by = b.police_state.PositionY;
		bz = b.police_state.PositionZ;
	}

	float dx = ax - bx;
	float dy = ay - by;
	float dz = az - bz;

	return dx * dx + dy * dy + dz * dz; // 3D 거리 제곱
}

template <typename PacketT>
void broadcast_in_room(int sender_id, int room_id, const PacketT* packet, float view_range = -1.0f) {
	if (room_id < 0 || room_id >= static_cast<int>(g_rooms.size())) {
		std::cout << "Invalid room ID: " << room_id << std::endl;
		return;
	}

	const Room& r = g_rooms[room_id];

	float view_range_sq = (view_range > 0) ? view_range * view_range : -1.0f;	// -1이면 전원에게 전송
	
	for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i) {
		uint8_t other_id = r.player_ids[i];
		if (other_id == UINT8_MAX || other_id == sender_id)
			continue;

		if (!g_users.count(other_id) || !g_users[other_id].isValidSocket())
			continue;
		g_users[other_id].do_send_packet(reinterpret_cast<void*>(const_cast<PacketT*>(packet)));
		/*
		// 거리 체크
		bool inRange = (view_range_sq < 0) || (distanceSq(g_users[sender_id], g_users[other_id]) <= view_range_sq);
		// 시야 안에 들어온 경우
		if (inRange) {
			// 원래 안 보였는데 새로 보이게 됨 -> appear 이벤트 전송
			if (g_users[other_id].visible_ids.insert(sender_id).second) {
				IdOnlyPacket appear;
				appear.header = appearHeader;
				appear.size = sizeof(IdOnlyPacket);
				appear.id = static_cast<uint8_t>(sender_id);
				g_users[other_id].do_send_packet(&appear);
			}

			// 상태 패킷 전송
			g_users[other_id].do_send_packet(reinterpret_cast<void*>(const_cast<PacketT*>(packet)));
		}
		// 시야 밖으로 나간 경우
		else {
			// 원래 보이던 애가 사라짐 -> disappear 이벤트 전송
			if (g_users[other_id].visible_ids.erase(sender_id) > 0) {
				IdOnlyPacket disappear;
				disappear.header = disappearHeader;
				disappear.size = sizeof(IdOnlyPacket);
				disappear.id = static_cast<uint8_t>(sender_id);
				g_users[other_id].do_send_packet(&disappear);
			}

			// 상태 패킷 전송
			g_users[other_id].do_send_packet(reinterpret_cast<void*>(const_cast<PacketT*>(packet)));
		}
		*/
	}
}

bool validate_cultist_state(FCultistCharacterState& state)
{
	bool ok = true;

	// 속도 체크
	if (state.Speed > MAX_SPEED) {
		std::cout << "[AntiCheat] Player " << state.PlayerID
			<< " exceeded speed: " << state.Speed
			<< " -> clamped to " << MAX_SPEED << std::endl;
		state.Speed = MAX_SPEED;
		ok = false;
	}

	// 죽은 상태인데 Speed > 0
	if (state.ABP_IsDead && state.Speed > 0.0f) {
		std::cout << "[AntiCheat] Player " << state.PlayerID
			<< " is dead but moving. Speed=" << state.Speed << std::endl;
		state.Speed = 0.0f;
		ok = false;
	}

	// Crouch 속도 체크
	if (state.Crouch && state.Speed > MAX_SPEED / 2) {
		std::cout << "[AntiCheat] Player " << state.PlayerID
			<< " exceeded speed: " << state.Speed
			<< " -> clamped to " << MAX_SPEED / 2 << std::endl;
		state.Speed = MAX_SPEED / 2;
		ok = false;
	}

	// 맵 범위 체크 + 음수체크 
	if (state.PositionX < -MAP_BOUND_X || state.PositionX > MAP_BOUND_X ||
		state.PositionY < -MAP_BOUND_Y || state.PositionY > MAP_BOUND_Y ||
		state.PositionZ < MAP_MIN_Z)
	{
		std::cout << "[AntiCheat] Player " << state.PlayerID
			<< " out of map bounds: ("
			<< state.PositionX << ", " << state.PositionY << ", " << state.PositionZ << ")\n";
		ok = false;
	}
	return ok;
}

bool validate_police_state(FPoliceCharacterState& state)
{
	bool ok = true;

	// 속도 체크
	if (state.Speed > MAX_SPEED) {
		std::cout << "[AntiCheat] Police " << state.PlayerID
			<< " exceeded speed: " << state.Speed
			<< " -> clamped to " << MAX_SPEED << std::endl;
		state.Speed = MAX_SPEED;
		ok = false;
	}

	// Crouch 속도 체크
	if (state.bIsCrouching && state.Speed > MAX_SPEED / 2) {
		std::cout << "[AntiCheat] Player " << state.PlayerID
			<< " exceeded speed: " << state.Speed
			<< " -> clamped to " << MAX_SPEED / 2 << std::endl;
		state.Speed = MAX_SPEED / 2;
		ok = false;
	}

	// 맵 범위 체크
	if (state.PositionX < -MAP_BOUND_X || state.PositionX > MAP_BOUND_X ||
		state.PositionY < -MAP_BOUND_Y || state.PositionY > MAP_BOUND_Y ||
		state.PositionZ < MAP_MIN_Z)
	{
		std::cout << "[AntiCheat] Police " << state.PlayerID
			<< " out of map bounds: ("
			<< state.PositionX << ", " << state.PositionY << ", " << state.PositionZ << ")\n";
		ok = false;
	}

	return ok;
}

bool validate_dog_state(int c_id, Dog dog)
{
	bool ok = true;
	// 주인 체크
	if (dog.owner != c_id) {
		std::cout << "[AntiCheat] owner: " << dog.owner << "'is not same in packet: ("
			<< dog.owner << ")\n";
		ok = false;
	}
	// 맵 범위 체크
	if (dog.loc.x < -MAP_BOUND_X || dog.loc.x > MAP_BOUND_X ||
		dog.loc.y < -MAP_BOUND_Y || dog.loc.y > MAP_BOUND_Y ||
		dog.loc.z < MAP_MIN_Z)
	{
		std::cout << "[AntiCheat] owner: " << dog.owner << "'s dog out of map bounds : ("
			<< dog.loc.x << ", " << dog.loc.y << ", " << dog.loc.z << ")\n";
		ok = false;
	}

	return ok;
}

std::optional<int> SphereTraceClosestCultist(int centerId, float radius) {
	auto myIt = g_users.find(centerId);
	if (myIt == g_users.end()) {
		std::cout << "Error Cid in SphereTraceClosestCultist: " << centerId << "\n";
		return std::nullopt;
	}

	const SESSION& me = myIt->second;
	const int roomId = me.room_id;
	if (roomId < 0 || roomId >= MAX_ROOM) {
		std::cout << "User " << centerId << " room id error " << "\n";
		return std::nullopt;
	}
	const Room& rm = g_rooms[roomId];

	const float r2 = radius * radius;
	float nearestId = std::numeric_limits<float>::max();

	std::optional<int> bestId;

	for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i)
	{
		int other = rm.player_ids[i];
		if (other == UINT8_MAX || other == centerId) 
			continue;

		auto it = g_users.find(other);
		if (it == g_users.end()) 
			continue;

		const SESSION& target = it->second;

		// 소켓/상태/역할 필터
		if (!target.isValidSocket())   
			continue;
		if (target.role != 0)           // Cultist
			continue;
		// 치료 가능한 상태 확인

		const float d2 = distanceSq(me, target);
		if (d2 <= r2 && d2 < nearestId) {
			nearestId = d2;
			bestId = other;
		}
	}
	return bestId;
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

bool line_sphere_intersect(
	const FVector& start, const FVector& end,
	const FVector& center, double radius)
{
	FVector d{
		end.x - start.x,
		end.y - start.y,
		end.z - start.z
	};

	FVector f{
		start.x - center.x,
		start.y - center.y,
		start.z - center.z
	};

	double a = d.x * d.x + d.y * d.y + d.z * d.z;
	double b = 2.0 * (f.x * d.x + f.y * d.y + f.z * d.z);
	double c = (f.x * f.x + f.y * f.y + f.z * f.z) - (radius * radius);

	double discriminant = b * b - 4.0 * a * c;

	if (discriminant < 0.0)
		return false; // 충돌 없음

	discriminant = std::sqrt(discriminant);

	double t1 = (-b - discriminant) / (2.0 * a);
	double t2 = (-b + discriminant) / (2.0 * a);

	return (t1 >= 0.0 && t1 <= 1.0) ||
		(t2 >= 0.0 && t2 <= 1.0);
}

FVector rotation_to_forward(double pitchDeg, double yawDeg)
{
	double pitch = pitchDeg * PI / 180.0;
	double yaw = yawDeg * PI / 180.0;

	double cp = cos(pitch);
	double sp = sin(pitch);
	double cy = cos(yaw);
	double sy = sin(yaw);

	return FVector{
		cp * cy,
		cp * sy,
		sp
	};
}

void baton_sweep(int c_id, HitPacket* p)
{
	auto& attacker = g_users[c_id];
	int room = attacker.room_id;
	if (room < 0)
		return;

	FVector start{ p->TraceStart.x, p->TraceStart.y, p->TraceStart.z };
	FVector forward{ p->TraceDir.x, p->TraceDir.y, p->TraceDir.z };

	double range = 200.0;
	// 유저간 충돌 전 지형 충돌 검사
	Ray ray{ start.x, start.y, start.z ,forward.x, forward.y, forward.z };
	float mapHitDist;
	int mapTri;

	/*std::cout << "[BatonSweep] ray start = ("
		<< ray.ox << ", "
		<< ray.oy << ", "
		<< ray.oz << ") dir = ("
		<< ray.dx << ", "
		<< ray.dy << ", "
		<< ray.dz << ")\n";
	std::cout << "[MapAABB] X "
		<< g_mapAABB.minX << " ~ " << g_mapAABB.maxX
		<< " Y " << g_mapAABB.minY << " ~ " << g_mapAABB.maxY
		<< " Z " << g_mapAABB.minZ << " ~ " << g_mapAABB.maxZ << "\n";*/

	if (LineTraceMap(
		ray, static_cast<float>(range),
		g_mapTris, g_triAABBs, g_grid,
		g_mapAABB, cellSize, mapHitDist, mapTri))
	{
		std::cout << "LineTraceMap." << std::endl;
		range = mapHitDist;
	}
	else{
		std::cout << "!LineTraceMap." << std::endl;
	}

	FVector end = start + forward * range;

	for (int otherId : g_rooms[room].player_ids)
	{
		if (otherId == c_id || g_users[otherId].role != 0 || otherId == UINT8_MAX)
			continue;

		auto& target = g_users[otherId];
		if (!target.isValidSocket())
			continue;

		FVector targetPos{
			target.cultist_state.PositionX,
			target.cultist_state.PositionY,
			target.cultist_state.PositionZ
		};

		if (line_sphere_intersect(start, end, targetPos, 50.0))
		{
			std::cout << "line_sphere_intersect." << std::endl;
			HitResultPacket result;
			result.header = hitHeader;
			result.size = sizeof(HitResultPacket);
			result.AttackerID = c_id;
			result.TargetID = otherId;
			result.Weapon = EWeaponType::Baton;

			broadcast_in_room(c_id, room, &result, VIEW_RANGE);
			return;
		}
	}
}

void line_trace(int c_id, HitPacket* p) {
	auto& attacker = g_users[c_id];
	int room = attacker.room_id;
	if (room < 0) 
		return;

	FVector start{ p->TraceStart.x, p->TraceStart.y, p->TraceStart.z };
	FVector dir{ p->TraceDir.x,   p->TraceDir.y,   p->TraceDir.z };
	double range = (p->Weapon == EWeaponType::Taser) ? 1000.0 : 10000.0;

	Ray ray{ start.x, start.y, start.z ,dir.x, dir.y, dir.z };
	float mapHitDist;
	int mapTri;

	if (LineTraceMap(
		ray, static_cast<float>(range),
		g_mapTris, g_triAABBs, g_grid,
		g_mapAABB, cellSize, mapHitDist, mapTri))
	{
		range = mapHitDist;
	}

	FVector end = start + dir * range;

	for (int otherId : g_rooms[room].player_ids)
	{
		if (otherId == c_id || g_users[otherId].role != 0) 
			continue;

		auto& target = g_users[otherId];
		if (!target.isValidSocket()) 
			continue;

		FVector targetPos{ 
			target.cultist_state.PositionX, 
			target.cultist_state.PositionY, 
			target.cultist_state.PositionZ 
		};

		if (line_sphere_intersect(start, end, targetPos, 60.0))
		{
			HitResultPacket result;
			result.header = hitHeader;
			result.size = sizeof(HitResultPacket);
			result.AttackerID = c_id;
			result.TargetID = otherId;
			result.Weapon = p->Weapon;

			broadcast_in_room(c_id, room, &result, VIEW_RANGE);
			return;
		}
	}
}

void process_packet(int c_id, char* packet) {
	switch (packet[0]) {
	case cultistHeader:
	{
		auto* p = reinterpret_cast<CultistPacket*>(packet);
		if (p->size != sizeof(CultistPacket)) {
			std::cout << "Invalid CultistPacket size\n";
			break;
		}
		//if (!validate_cultist_state(p->state)) {
		//	std::cout << "Suspicious cultist packet from c_id " << c_id << std::endl;
		//}
		g_users[c_id].cultist_state = p->state;

		broadcast_in_room(c_id, g_users[c_id].room_id, p, VIEW_RANGE);
		break;
	}
	case treeHeader: 
	{
		auto* p = reinterpret_cast<TreePacket*>(packet);
		if (p->size != sizeof(TreePacket)) {
			std::cout << "Invalid TreePacket size\n";
			break;
		}
		std::cout << "[TreeRecv] from=" << (int)c_id << " room=" << g_users[c_id].room_id << "\n";
		
		broadcast_in_room(c_id, g_users[c_id].room_id, p, VIEW_RANGE);
		break;
	}
	case crowSpawnHeader:
	{
		auto* p = reinterpret_cast<CrowPacket*>(packet);
		if (p->size != sizeof(CrowPacket)) {
			std::cout << "Invalid SkillCrowPacket size\n";
			break;
		}
		std::cout << "[CrowSpawnRecv] from=" << p->crow.owner << " room=" << g_users[c_id].room_id << "\n";
		g_users[c_id].crow = p->crow;
		if (!g_users[c_id].crow.is_alive) {
			break;
		}

		broadcast_in_room(c_id, g_users[c_id].room_id, p, VIEW_RANGE);
		break;
	}
	case crowDataHeader:
	{
		auto* p = reinterpret_cast<CrowPacket*>(packet);
		if (p->size != sizeof(CrowPacket)) {
			std::cout << "Invalid CrowPacket size\n";
			break;
		}
		g_users[c_id].crow = p->crow;
		if (!g_users[c_id].crow.is_alive) {
			break;
		}

		broadcast_in_room(c_id, g_users[c_id].room_id, p, VIEW_RANGE);
		break;
	}
	case crowDisableHeader:
	{
		auto* p = reinterpret_cast<IdOnlyPacket*>(packet);
		if (p->size != sizeof(IdOnlyPacket)) {
			std::cout << "Invalid IdOnlyPacket size\n";
			break;
		}
		std::cout << "[CrowDisableRecv] from=" << (int)p->id << " room=" << g_users[c_id].room_id << "\n";
		g_users[c_id].crow.is_alive = false;

		broadcast_in_room(c_id, g_users[c_id].room_id, p, VIEW_RANGE);
		break;
	}
	case policeHeader:
	{
		auto* p = reinterpret_cast<PolicePacket*>(packet);
		if (p->size != sizeof(PolicePacket)) {
			std::cout << "Invalid PolicePacket size\n";
			break;
		}
		//if (!validate_police_state(p->state)) {
		//	std::cout << "Suspicious police packet from c_id " << c_id << std::endl;
		//}
		g_users[c_id].police_state = p->state;

		broadcast_in_room(c_id, g_users[c_id].room_id, p, VIEW_RANGE);
		break;
	}
	case dogHeader:
	{
		auto* p = reinterpret_cast<DogPacket*>(packet);
		if (p->size != sizeof(DogPacket)) {
			std::cout << "Invalid DogPacket size\n";
			break;
		}
		//if (!validate_dog_state(c_id, p->dog)) {
		//	std::cout << "Suspicious dog packet from c_id " << c_id << std::endl;
		//}
		g_users[c_id].dog = p->dog;

		broadcast_in_room(c_id, g_users[c_id].room_id, p, VIEW_RANGE);
		break;
	}
	case particleHeader:
	{
		auto* p = reinterpret_cast<ParticlePacket*>(packet);
		if (p->size != sizeof(ParticlePacket)) {
			std::cout << "Invalid ParticlePacket size\n";
			break;
		}

		int room_id = g_users[c_id].room_id;
		if (room_id < 0 || room_id >= static_cast<int>(g_rooms.size()))
		{
			std::cout << "Invalid room ID: " << room_id << std::endl;
			break;
		}

		broadcast_in_room(c_id, g_users[c_id].room_id, p, VIEW_RANGE);
		break;
	}
	case hitHeader:
	{
		auto* p = reinterpret_cast<HitPacket*>(packet);
		if (p->size != sizeof(HitPacket)) {
			std::cout << "Invalid HitPacket size\n";
			break;
		}

		switch (p->Weapon)
		{
		case EWeaponType::Baton:
		{
			baton_sweep(c_id, p);
			break;
		}
		case EWeaponType::Taser:
		case EWeaponType::Pistol:
		{
			line_trace(c_id, p);
			break;
		}
		default:
			break;
		}
		broadcast_in_room(c_id, g_users[c_id].room_id, p, VIEW_RANGE);
		break;
	}
	case tryHealHeader: 
	{
		auto* p = reinterpret_cast<IdOnlyPacket*>(packet);
		if (p->size != sizeof(IdOnlyPacket)) {
			std::cout << "Invalid IdOnlyPacket size\n";
			break;
		}

		// 근처 치료 가능한 유저 탐색
		auto targetOpt = SphereTraceClosestCultist(c_id, SPHERE_TRACE_RADIUS);
		if (!targetOpt) {
			// 타겟 없음
			break;
		}

		const int targetId = *targetOpt;
		std::cout << "[Heal] healer=" << c_id << " target=" << (int)targetId << "\n";
		if (g_users[c_id].cultist_state.ABP_DoHeal ||
			g_users[targetId].cultist_state.ABP_GetHeal)
		{
			std::cout << "Heal already in progress\n";
			break;
		}
		// location과 rotation을 계산
		auto moveOpt = GetMovePoint(c_id, targetId);
		// 두 플레이어에게 이동해야할 위치를 각각 전송
		auto [moveLoc, moveRot] = *moveOpt;

		MovePacket pkt;
		pkt.header = doHealHeader;
		pkt.size = sizeof(MovePacket);

		const double rad = moveRot.yaw * PI / 180.0;
		const double dirX = std::cos(rad);
		const double dirY = std::sin(rad);

		FVector healerPos{
			moveLoc.x - dirX * (HEAL_GAP * 0.5),
			moveLoc.y - dirY * (HEAL_GAP * 0.5),
			moveLoc.z
		};
		pkt.SpawnLoc = healerPos;
		pkt.SpawnRot = moveRot;
		pkt.isHealer = true;
		g_users[c_id].heal_partner = targetId;
		g_users[c_id].do_send_packet(&pkt);

		double yaw = std::fmod(moveRot.yaw + 180.0, 360.0);
		if (yaw < 0.0) { 
			yaw += 360.0; 
		}

		FVector targetPos{
			moveLoc.x + dirX * (HEAL_GAP * 0.5),
			moveLoc.y + dirY * (HEAL_GAP * 0.5),
			moveLoc.z
		};
		pkt.SpawnLoc = targetPos;
		pkt.SpawnRot.yaw = yaw;
		pkt.isHealer = false;
		g_users[targetId].heal_partner = c_id;
		g_users[targetId].do_send_packet(&pkt);

		TIMER_EVENT ev;
		ev.wakeup_time = std::chrono::system_clock::now() + 1s;
		ev.c_id = c_id;
		ev.target_id = targetId;
		ev.event_id = EV_HEAL;
		timer_queue.push(ev);
		break;
	}
	case endHealHeader:
	{
		auto* p = reinterpret_cast<NoticePacket*>(packet);
		if (p->size != sizeof(NoticePacket)) {
			std::cout << "Invalid NoticePacket size\n";
			break;
		}
		int heal_partner{ g_users[c_id].heal_partner };
		g_users[heal_partner].heal_partner = -1;
		g_users[c_id].heal_partner = -1;

		BoolPacket packet;
		packet.header = endHealHeader;
		packet.result = false;
		packet.size = sizeof(BoolPacket);
		g_users[heal_partner].do_send_packet(&packet);
		break;
	}
	case ritualStartHeader: 
	{
		auto* p = reinterpret_cast<RitualNoticePacket*>(packet);
		if (p->size != sizeof(RitualNoticePacket)) {
			std::cout << "Invalid RitualNoticePacket size\n";
			break;
		}

		int room_id = g_users[c_id].room_id;
		if (room_id < 0 || room_id >= MAX_ROOM) 
			break;

		uint8_t ritual_id = p->ritual_id;
		if (ritual_id >= 5) 
			break;

		auto& altar = g_altars[room_id][ritual_id];
		altar.isActivated = true;
		altar.time = std::chrono::system_clock::now();

		std::cout << "[RitualStart] cultist=" << c_id << " altar=" << (int)ritual_id << "gauge= " << altar.gauge << "\n";
		// 0퍼면 냅두고 아니면 클라한테 몇펀지 보내주기
		break;
	}
	case ritualDataHeader:
	{
		auto* p = reinterpret_cast<RitualNoticePacket*>(packet);
		if (p->size != sizeof(RitualNoticePacket)) {
			std::cout << "Invalid RitualNoticePacket size\n";
			break;
		}

		int room_id = g_users[c_id].room_id;
		if (room_id < 0 || room_id >= MAX_ROOM) 
			break;
		uint8_t ritual_id = p->ritual_id;
		if (ritual_id >= 5) 
			break;

		auto& altar = g_altars[room_id][ritual_id];

		if (!altar.isActivated)
			break;

		auto now = std::chrono::system_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - altar.time).count();
		if (elapsed > 0) {
			int add = static_cast<int>(elapsed) * 10;
			altar.gauge = std::min(100, altar.gauge + add);
			altar.time = now;
		}
		// reason: 1 -> 성공 / 2 -> 실패
		if (p->reason == 1)
			altar.gauge = std::min(100, altar.gauge + 10);
		else if (p->reason == 2)
			altar.gauge = std::max(0, altar.gauge - 10);		

		RitualGagePacket gauge{};
		gauge.header = ritualDataHeader;
		gauge.size = sizeof(RitualGagePacket);
		gauge.ritual_id = ritual_id;
		gauge.gauge = altar.gauge;
		g_users[c_id].do_send_packet(&gauge);

		// broadcast_in_room(c_id, room_id, &gauge);
		std::cout << "[RitualData] altar=" << (int)ritual_id << " gauge=" << altar.gauge << "\n";
		break;
	}
	case ritualEndHeader:
	{
		auto* p = reinterpret_cast<RitualNoticePacket*>(packet);
		if (p->size != sizeof(RitualNoticePacket)) {
			std::cout << "Invalid RitualNoticePacket size\n";
			break;
		}

		int room_id = g_users[c_id].room_id;
		if (room_id < 0 || room_id >= MAX_ROOM) 
			break;

		uint8_t ritual_id = p->ritual_id;
		if (ritual_id >= 5) 
			break;
		if (p->reason == 3) {
			auto& altar = g_altars[room_id][ritual_id];
			auto now = std::chrono::system_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - altar.time).count();
			if (elapsed > 0) {
				int add = static_cast<int>(elapsed / 100); // 0.1초당 1% 증가
				altar.gauge = std::min(100, altar.gauge + add);
				altar.time = now;
			}
			if (altar.gauge >= 100) {
				RitualNoticePacket packet;
				packet.header = ritualEndHeader;
				packet.size = sizeof(RitualNoticePacket);
				packet.id = c_id;
				packet.ritual_id = ritual_id;
				packet.reason = 4;
				g_users[c_id].do_send_packet(&packet);
			}
			altar.isActivated = false;
			std::cout << "[RitualEnd] cultist=" << c_id << " altar=" << (int)ritual_id << " gauge: " << altar.gauge << "\n";
		}
		if (p->reason == 4) {
			auto& altar = g_altars[room_id][ritual_id];
			auto now = std::chrono::system_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - altar.time).count();
			if (elapsed > 0) {
				int add = static_cast<int>(elapsed) * 10;
				altar.gauge = std::min(100, altar.gauge + add);
				altar.time = now;
			}
			if (altar.gauge >= 98) {
				RitualNoticePacket packet;
				packet.header = ritualEndHeader;
				packet.size = sizeof(RitualNoticePacket);
				packet.id = c_id;
				packet.ritual_id = ritual_id;
				packet.reason = 4;
				g_users[c_id].do_send_packet(&packet);
			}
			else {
				RitualNoticePacket packet;
				packet.header = ritualEndHeader;
				packet.size = sizeof(RitualNoticePacket);
				packet.id = c_id;
				packet.ritual_id = ritual_id;
				packet.reason = altar.gauge;
				g_users[c_id].do_send_packet(&packet);
			}
			altar.isActivated = false;
			std::cout << "[RitualEnd] cultist=" << c_id << " altar=" << (int)ritual_id << " gauge: " << altar.gauge << "\n";
		}
		break;
	}
	case connectionHeader:
	{
		auto* p = reinterpret_cast<IdOnlyPacket*>(packet);
		if (p->size != sizeof(IdOnlyPacket)) {
			std::cout << "Invalid ConnectionPacket size\n";
			break;
		}
		
		std::cout << "Client[" << c_id << "] connected" << std::endl;
		// 새로 접속한 유저(id)에게 본인 id 확정 send
		g_users[c_id].do_send_packet(p);
		break;
	}
	case DisconnectionHeader: 
	{
		std::lock_guard<std::mutex> lk(g_room_mtx);
		g_room_q.push(RoomTask{ c_id, RM_QUIT, g_users[c_id].role, g_users[c_id].room_id });
		g_room_cv.notify_one();
		break;
	}
	case disableHeader:
	{
		g_users[c_id].setState(ST_DISABLE);

		int room_id = g_users[c_id].room_id;
		if (room_id < 0 || room_id >= static_cast<int>(g_rooms.size())) {
			std::cout << "Invalid room ID in disableHeader\n";
			break;
		}

		bool allCultistsDisabled = true;

		const Room& r = g_rooms[room_id];
		for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i)
		{
			uint8_t pid = r.player_ids[i];
			if (pid == UINT8_MAX) continue;

			if (g_users.count(pid) && g_users[pid].getRole() == 0 && g_users[pid].getState() != ST_DISABLE) {
				allCultistsDisabled = false;
				break;
			}
		}

		if (allCultistsDisabled)
		{
			std::cout << "[Server] All cultists are disabled in Room[" << room_id << "]\n";
			std::cout << "[Server] Police Win!\n";

			IdOnlyPacket newPacket;
			newPacket.header = DisconnectionHeader;
			newPacket.size = sizeof(IdOnlyPacket);

			std::vector<int> ids_to_disconnect;
			for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i)
			{
				uint8_t pid = r.player_ids[i];
				if (pid == UINT8_MAX) continue;

				if (g_users.count(pid) && g_users[pid].isValidSocket())
				{
					newPacket.id = pid;
					g_users[pid].do_send_packet(&newPacket);
					ids_to_disconnect.push_back(pid);
				}
			}

			for (int id : ids_to_disconnect)
			{
				disconnect(id);
			}
			// 매치가 끝났으니 room을 초기화
			g_rooms[room_id].cultist = 0;
			g_rooms[room_id].police = 0;
			for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i) {
				g_rooms[room_id].player_ids[i] = UINT8_MAX;
			}
			g_rooms[room_id].isIngame = false;
		}
		break;
	}
	case requestHeader: 
	{ 
		auto* p = reinterpret_cast<RoleOnlyPacket*>(packet);
		if (p->size != sizeof(RoleOnlyPacket)) {
			std::cout << "Invalid requestHeader size\n";
			break;
		}
		int role = static_cast<int>(p->role);
		{
			std::lock_guard<std::mutex> lk(g_room_mtx);
			g_room_q.push(RoomTask{ c_id, RM_REQ, role, -1 });
		}
		g_room_cv.notify_one();
		break;
	}
	case enterHeader: 
	{
		auto* p = reinterpret_cast<RoomNumberPacket*>(packet);
		if (p->size != sizeof(RoomNumberPacket)) {
			std::cout << "Invalid enterPacket size\n";
			break;
		}
		int room_id = p->room_number;
		{
			std::lock_guard<std::mutex> lk(g_room_mtx);
			g_room_q.push(RoomTask{ c_id, RM_ENTER, g_users[c_id].role, room_id });
		}
		g_room_cv.notify_one();
		break;
	}
	case leaveHeader: 
	{
		// 1. 방에서 나갔으니까 page가 0인 RoomdataPakcet 전송.
		// 2. room.player_ids에서 해당 id 제거 (=-1)
		// 3. 역할에 따라 room.police 또는 cultist--
		// 4. isIngame = false;
		break;
	}
	case readyHeader:
	{
		// 1. player 상태를 ready로 변경
		// 2. InRoomPacket으로 방 상태 업데이트를 방 전원에게 broadcast
		g_users[c_id].setState(ST_READY);
		break;
	}
	case gameStartHeader: 
	{
		auto* p = reinterpret_cast<RoomNumberPacket*>(packet);
		if (p->size != sizeof(RoomNumberPacket)) {
			std::cout << "Invalid gameStartHeader size\n";
			break;
		}
		int room_id = p->room_number;
		{
			std::lock_guard<std::mutex> lk(g_room_mtx);
			g_room_q.push(RoomTask{ c_id, RM_GAMESTART, g_users[c_id].role, room_id});
		}
		g_room_cv.notify_one();
		break;
	}
	case ritualHeader: 
	{
		std::lock_guard<std::mutex> lk(g_room_mtx);
		g_room_q.push(RoomTask{ c_id, RM_RITUAL, 0, -1 });
		g_room_cv.notify_one();
		break;
	}
	case loginHeader:
	{
		auto* p = reinterpret_cast<IdPwPacket*>(packet);
		if (p->size != sizeof(IdPwPacket)) {
			std::cout << "Invalid LoginPacket size\n";
			break;
		}
		std::string uid{ p->id };
		std::string upw{ p->pw };
		{
			std::lock_guard<std::mutex> lk(g_db_mtx);
			g_db_q.push(DBTask{ c_id, EV_LOGIN, std::move(uid), std::move(upw) });
		}
		g_db_cv.notify_one();
		break;
	}
	case idExistHeader:
	{
		auto* p = reinterpret_cast<IdPacket*>(packet);
		if (p->size != sizeof(IdPacket)) {
			std::cout << "Invalid LoginPacket size\n";
			break;
		}
		std::string uid{ p->id };
		{
			std::lock_guard<std::mutex> lk(g_db_mtx);
			g_db_q.push(DBTask{ c_id, EV_EXIST, std::move(uid), {} });
		}
		g_db_cv.notify_one();
		break;
	}
	case signUpHeader:
	{
		auto* p = reinterpret_cast<IdPwPacket*>(packet);
		if (p->size != sizeof(IdPwPacket)) {
			std::cout << "Invalid signUpPacket size\n";
			break;
		}
		std::string uid{ p->id };
		std::string upw{ p->pw };
		{
			std::lock_guard<std::mutex> lk(g_db_mtx);
			g_db_q.push(DBTask{ c_id, EV_SIGNUP, std::move(uid), std::move(upw) });
		}
		g_db_cv.notify_one();
		break;
	}
	case quitHeader:
	{
		std::lock_guard<std::mutex> lk(g_room_mtx);
		g_room_q.push(RoomTask{ c_id, RM_DISCONNECT, g_users[c_id].role, g_users[c_id].room_id });
		g_room_cv.notify_one();
		break;
	}
	default:
		char header = packet[0];
		std::cout << "invalidHeader From id: " << c_id << "header: " << header << std::endl;
	}
}

void mainLoop(HANDLE h_iocp) {
	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* o = nullptr;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &o, INFINITE);
		EXP_OVER* eo = reinterpret_cast<EXP_OVER*>(o);

		if (FALSE == ret) {
			if (eo->comp_type == OP_ACCEPT) 
				std::cout << "Accept Error";
			else {
				std::cout << "GQCS Error on client[" << key << "]\n";
				disconnect(static_cast<int>(key));
				if (eo->comp_type == OP_SEND) 
					delete eo;
				continue;
			}
		}
		if ((0 == num_bytes) && ((eo->comp_type == OP_RECV) || (eo->comp_type == OP_SEND))) {
			disconnect(static_cast<int>(key));
			if (eo->comp_type == OP_SEND) delete eo;
			continue;
		}
		switch (eo->comp_type)
		{
		case OP_ACCEPT:
		{
			int new_id = client_id++;
			SESSION sess;
			sess.id = new_id;
			sess.prev_remain = 0;
			sess.state = ST_FREE;
			sess.c_socket = g_c_socket;
			sess.role = -1;
			sess.room_id = -1;
			sess.heal_gage = 0;
			g_users.insert(std::make_pair(new_id, sess));	// 여기 emplace로 바꿀 순 없을까?
			CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket), h_iocp, new_id, 0);
			g_users[new_id].do_recv();

			SOCKADDR_IN* client_addr = (SOCKADDR_IN*)(g_a_over.send_buffer + sizeof(SOCKADDR_IN) + 16);
			std::cout << "클라이언트 접속: IP = " << inet_ntoa(client_addr->sin_addr) << ", Port = " << ntohs(client_addr->sin_port) << std::endl;
			g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

			ZeroMemory(&g_a_over.over, sizeof(g_a_over.over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(g_s_socket, g_c_socket, g_a_over.send_buffer, 0, addr_size + 16, addr_size + 16, 0, &g_a_over.over);
			break;
		}
		case OP_RECV: {
			int remain_data = num_bytes + g_users[key].prev_remain;
			char* p = eo->send_buffer;
			while (remain_data > 0) {
				int packet_size = p[1];
				if (packet_size <= remain_data) {
					process_packet(static_cast<int>(key), p);
					p = p + packet_size;
					remain_data = remain_data - packet_size;
				}
				else break;
			}
			g_users[key].prev_remain = remain_data;
			if (remain_data > 0) {
				memcpy(eo->send_buffer, p, remain_data);
			}
			g_users[key].do_recv();
			break;
		}
		case OP_SEND:
		{
			delete eo;
			break;
		}
		case OP_LOGIN:
		{
			auto* pkt = reinterpret_cast<BoolPacket*>(eo->send_buffer);
			int cid = static_cast<int>(key);

			if (pkt->result && g_users.count(cid)) {
				g_users[cid].account_id = eo->account_id;
			}

			if (g_users.count(cid) && g_users[cid].isValidSocket())
				g_users[cid].do_send_packet(pkt);

			delete eo;
			break;
		}
		case OP_ID_EXIST:
		case OP_SIGNUP:
		{
			// eo->send_buffer 안에 BoolPacket이 있음
			// key는 c_id
			if (g_users.count((int)key) && g_users[(int)key].isValidSocket()) {
				g_users[(int)key].do_send_packet(eo->send_buffer);
			}
			delete eo;
			break;
		}
		case OP_ROOM_REQ:
		case OP_ROOM_ENTER:
		case OP_ROOM_GAMESTART:
		case OP_ROOM_RITUAL:
		case OP_ROOM_DISCONNECT:
		{
			int cid = static_cast<int>(key);
			if (g_users.count(cid) && g_users[cid].isValidSocket())
				g_users[cid].do_send_packet(eo->send_buffer);

			delete eo;
			break;
		}
		}
	}
}

int main()
{
	if (!LoadOBJAndComputeAABB("SM_MERGED_StaticMeshActor_NewmapLandmass_transformBake.OBJ", g_mapVertices, g_mapTriangles, g_mapAABB))
	{
		std::cout << "OBJ load failed\n";
		return 1;
	}

	std::cout << "vertices = " << g_mapVertices.size() << "\n";
	std::cout << "triangles = " << g_mapTriangles.size() << "\n";

	std::cout << "AABB min = ("
		<< g_mapAABB.minX << ", "
		<< g_mapAABB.minY << ", "
		<< g_mapAABB.minZ << ")\n";

	std::cout << "AABB max = ("
		<< g_mapAABB.maxX << ", "
		<< g_mapAABB.maxY << ", "
		<< g_mapAABB.maxZ << ")\n";

	std::cout << "v[0] = ("
		<< g_mapVertices[0].x << ", "
		<< g_mapVertices[0].y << ", "
		<< g_mapVertices[0].z << ")\n";

	auto& t = g_mapTriangles[0];
	std::cout << "f[0] = ("
		<< t.v0 << ", "
		<< t.v1 << ", "
		<< t.v2 << ")\n";

	auto& t0 = g_mapTriangles[0];
	auto& a = g_mapVertices[t0.v0];
	auto& b = g_mapVertices[t0.v1];
	auto& c = g_mapVertices[t0.v2];

	std::cout << "tri[0]\n";
	std::cout << " A (" << a.x << ", " << a.y << ", " << a.z << ")\n";
	std::cout << " B (" << b.x << ", " << b.y << ", " << b.z << ")\n";
	std::cout << " C (" << c.x << ", " << c.y << ", " << c.z << ")\n";

	BuildTriangles(g_mapVertices, g_mapTriangles, g_mapTris);
	std::cout << "built tris = " << g_mapTris.size() << "\n";
	BuildTriangleAABBs(g_mapTris, g_triAABBs);
	std::cout << "tri AABBs = " << g_triAABBs.size() << "\n";
	BuildSpatialGrid(g_triAABBs, g_mapAABB, cellSize, g_grid);
	std::cout << "grid cells = " << g_grid.size() << "\n";

	Ray r{
		{ -8159.82f, 3051.3f, -3014.86f },
		{  0.911431f, -0.411453f, 0.0f }
	};

	float hitDist;
	int hitTri;

	if (LineTraceMap(r, 5000.0f, g_mapTris, g_triAABBs, g_grid,
		g_mapAABB, cellSize, hitDist, hitTri))
	{
		std::cout << "HIT dist=" << hitDist << " tri=" << hitTri << "\n";
	}
	else {
		std::cout << "NO HIT\n";
	}

	HANDLE h_iocp;
	std::wcout.imbue(std::locale("korean"));

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);

	//std::thread CommandThread(CommandWorker);

	g_s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	if (g_s_socket <= 0) {
		std::cout << "Failed to create socket" << std::endl;
		return 1;
	}
	else {
		std::cout << "Server Socket Created" << std::endl;
	}

	SOCKADDR_IN addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(g_s_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
	listen(g_s_socket, SOMAXCONN);
	INT addr_size = sizeof(SOCKADDR_IN);

	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_s_socket), h_iocp, 9999, 0);
	g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_a_over.comp_type = OP_ACCEPT;
	AcceptEx(g_s_socket, g_c_socket, g_a_over.send_buffer, 0, addr_size + 16, addr_size + 16, 0, &g_a_over.over);
	
	// db초기화
	//if (!InitializeDB()) {
	//	std::cout << "DB 초기화 실패, 프로그램 종료\n";
	//	return 0;
	//}

	for (int i = 0; i < 100; ++i) {
		g_rooms[i].room_id = i;
		InitializeAltars(i);
	}
	g_h_iocp = h_iocp;

	std::thread db_thread{ DBWorkerLoop };
	std::thread room_thread{ RoomWorkerLoop };
	std::thread timer_thread{ HealTimerLoop };
	std::thread command_thread{ CommandWorker };
	mainLoop(h_iocp);

	g_db_cv.notify_all();
	db_thread.join();
	g_room_cv.notify_all();
	room_thread.join();
	timer_thread.join();
	command_thread.join();


	//StopAIWorker();
	closesocket(g_s_socket);
	WSACleanup();
}
