#define _WINSOCK_DEPRECATED_NO_WARNINGS

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
#include "Protocol.h"
#include "error.h"
#include "PoliceAI.h"
#include "db.h"

#pragma comment(lib, "MSWSock.lib")
#pragma comment (lib, "WS2_32.LIB")

const float VIEW_RANGE = 1000.0f;           // 시야 반경
const float VIEW_RANGE_SQ = VIEW_RANGE * VIEW_RANGE;

SOCKET g_s_socket, g_c_socket;
EXP_OVER g_a_over;

std::atomic<int> client_id = 0;
std::unordered_map<int, SESSION> g_users;
std::array<room, 100> g_rooms;

int altar_locs[100][5];

// db event 큐
enum EVENT_TYPE { EV_LOGIN, EV_EXIST, EV_SIGNUP };
struct DBTask {
	int c_id;
	EVENT_TYPE event_id;
	std::string id;
	std::string pw;
};

std::mutex g_db_mtx;
std::condition_variable g_db_cv;
std::queue<DBTask> g_db_q;

std::mutex g_logged_mtx;
std::unordered_set<std::string> g_logged_in_ids;

HANDLE g_h_iocp = nullptr;
std::atomic<bool> g_db_worker_run{ true };
std::thread g_db_worker;

void DBWorkerLoop() {
	while (g_db_worker_run.load())
	{
		DBTask task;
		{
			std::unique_lock<std::mutex> lk(g_db_mtx);
			g_db_cv.wait(lk, [] { return !g_db_q.empty() || !g_db_worker_run.load(); });
			if (!g_db_worker_run.load()) break;
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

void CommandWorker()
{
	while (true)
	{
		std::string command;
		std::getline(std::cin, command);

		if (command == "add_ai")
		{
			int ai_id = client_id++;
			//InitializeAISession(ai_id);
			std::cout << "[Command] New AI added. ID: " << ai_id << "\n";
		}
		else if (command == "exit")
		{
			std::cout << "[Command] Exiting server.\n";
			exit(0);
		}
		else
		{
			std::cout << "[Command] Unknown command: " << command << "\n";
		}
	}
}

void disconnect(int c_id)
{
	std::cout << "socket disconnect" << g_users[c_id].c_socket << std::endl;
	{
		std::lock_guard<std::mutex> lk(g_logged_mtx);
		if (!g_users[c_id].account_id.empty())
			g_logged_in_ids.erase(g_users[c_id].account_id);
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

	const room& r = g_rooms[room_id];

	float view_range_sq = (view_range > 0) ? view_range * view_range : -1.0f;	// -1이면 전원에게 전송
	
	for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i) {
		uint8_t other_id = r.player_ids[i];
		if (other_id == UINT8_MAX || other_id == sender_id)
			continue;

		if (!g_users.count(other_id) || !g_users[other_id].isValidSocket())
			continue;

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
		}
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

void process_packet(int c_id, char* packet) {
	switch (packet[0]) {
	case cultistHeader:
	{
		auto* p = reinterpret_cast<CultistPacket*>(packet);
		if (p->size != sizeof(CultistPacket)) {
			std::cout << "Invalid CultistPacket size\n";
			break;
		}
		if (!validate_cultist_state(p->state)) {
			std::cout << "Suspicious cultist packet from c_id " << c_id << std::endl;
		}
		g_users[c_id].cultist_state = p->state;

		broadcast_in_room(c_id, g_users[c_id].room_id, p, VIEW_RANGE);
		break;
	}
	case skillHeader: 
	{
		auto* p = reinterpret_cast<SkillPacket*>(packet);
		if (p->size != sizeof(SkillPacket)) {
			std::cout << "Invalid SkillPacket size\n";
			break;
		}
		std::cout << "[SkillRecv] from=" << (int)c_id << " room=" << g_users[c_id].room_id
			<< " skill=" << (int)p->skill << " size=" << (int)p->size << "\n";

		p->casterId = static_cast<uint8_t>(c_id);

		int room_id = g_users[c_id].room_id;
		if (room_id < 0 || room_id >= static_cast<int>(g_rooms.size()))
		{
			std::cout << "Invalid room ID: " << room_id << std::endl;
			break;
		}

		const room& r = g_rooms[room_id];
		for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i)
		{
			uint8_t other_id = r.player_ids[i];
			if (other_id == UINT8_MAX || other_id == c_id)
				continue;

			if (g_users.count(other_id) && g_users[other_id].isValidSocket())
			{
				g_users[other_id].do_send_packet(p);
			}
		}
		break;
	}
	case policeHeader:
	{
		auto* p = reinterpret_cast<PolicePacket*>(packet);
		if (p->size != sizeof(PolicePacket)) {
			std::cout << "Invalid PolicePacket size\n";
			break;
		}
		if (!validate_police_state(p->state)) {
			std::cout << "Suspicious police packet from c_id " << c_id << std::endl;
		}
		g_users[c_id].police_state = p->state;

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

		broadcast_in_room(c_id, g_users[c_id].room_id, p, VIEW_RANGE);
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
		auto* p = reinterpret_cast<NoticePacket*>(packet);
		if (p->size != sizeof(NoticePacket)) {
			std::cout << "Invalid RoomNumberPacket size\n";
			break;
		}
		// 1. 받은 room_id의 c_id유저를 room에서 빼는 작업.
		// g_uers에서 빼는 작업은 mainloop에서 disconnect를 호출.
		// rooms에서 유저 role에 해당하는 role도 --
		int room_id = g_users[c_id].room_id;
		if (0 == g_users[c_id].role) {
			g_rooms[room_id].cultist--;
		}
		else if (1 == g_users[c_id].role) {
			g_rooms[room_id].police--;
		}
		for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i) {
			if (g_rooms[room_id].player_ids[i] == static_cast<uint8_t>(c_id)) {
				g_rooms[room_id].player_ids[i] = UINT8_MAX;
				break;
			}
		}
		// 2. room 멤버들에게 disconncection header로 유저의 접속을 알림.
		IdOnlyPacket packet;
		packet.header = DisconnectionHeader;
		packet.size = sizeof(IdOnlyPacket);
		packet.id = static_cast<uint8_t>(c_id);
		for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i) {
			if (g_rooms[room_id].player_ids[i] == UINT8_MAX) {
				continue;
			}
			uint8_t otherId = g_rooms[room_id].player_ids[i];
			g_users[otherId].do_send_packet(&packet);
		}
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

		const room& r = g_rooms[room_id];
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
		// 1. 클라이언트가 room data를 request했음.
		auto* p = reinterpret_cast<RoleOnlyPacket*>(packet);
		if (p->size != sizeof(RoleOnlyPacket)) {
			std::cout << "Invalid requestHeader size\n";
			break;
		}
		int role = static_cast<int>(p->role);
		g_users[c_id].setRole(role);
		// 2. 앞에서부터 ingame이 false이고, 
		//	  g_users[c_id].role의 플레이어가 full이 아닌 room을 RoomdataPakcet로 10개 전송
		RoomsPakcet packet;
		packet.header = requestHeader;
		packet.size = sizeof(RoomsPakcet);

		int inserted = 0;
		for (const room& room : g_rooms) {
			if (inserted >= 10)
				break;

			if (!room.isIngame)
			{
				if ((role == 1 && room.police < 1) ||
					(role == 0 && room.cultist < 4))
				{
					packet.rooms[inserted++] = room;
				}
			}
		}

		g_users[c_id].do_send_packet(&packet);
		break; 
	}
	case enterHeader: 
	{
		// 0. room number를 보고 비어있으면 Packet 전송
		// 1. 업데이트되어 룸이 입장 불가능하면 leaveHeader 전송
		auto* p = reinterpret_cast<RoomNumberPacket*>(packet);
		if (p->size != sizeof(RoomNumberPacket)) {
			std::cout << "Invalid enterPacket size\n";
			break;
		}
		int room_id = p->room_number;
		NoticePacket packet;
		switch (g_users[c_id].role)
		{
		case 0: // cultist
		{
			if (g_rooms[room_id].cultist >= MAX_CULTIST_PER_ROOM) {
				packet.header = leaveHeader;
				packet.size = sizeof(NoticePacket);
			}
			else {
				packet.header = enterHeader;
				packet.size = sizeof(NoticePacket);
			}
			break;
		}
		case 1: // police
		{
			if (g_rooms[room_id].police >= MAX_POLICE_PER_ROOM) {
				packet.header = leaveHeader;
				packet.size = sizeof(NoticePacket);
			}
			else {
				packet.header = enterHeader;
				packet.size = sizeof(NoticePacket);
			}
			break;
			
		}
		default:
			std::cout << "User " << c_id << " Has Strange role : " << g_users[c_id].role << std::endl;
			break;
		}
		g_users[c_id].do_send_packet(&packet);
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
		// 0. 유저 role에 따라 room의 유저 수 증가
		if (1 == g_users[c_id].role) {
			g_rooms[room_id].police++;
		}
		else if (0 == g_users[c_id].role) {
			g_rooms[room_id].cultist++;
		}
		// 1. room이 꽉차면 isIngame을 true로, 유저의 방번호 업데이트
		if (g_rooms[room_id].cultist >= MAX_CULTIST_PER_ROOM && g_rooms[room_id].police >= MAX_POLICE_PER_ROOM) {
			g_rooms[room_id].isIngame = true;
		}
		g_users[c_id].room_id = room_id;
		for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i) {
			if (g_rooms[room_id].player_ids[i] == UINT8_MAX) {
				g_rooms[room_id].player_ids[i] = static_cast<uint8_t>(c_id);
				break;
			}
		}
		// 2.내 아이디를 send해서 확정시켜주기
		IdRolePacket packet;
		packet.header = connectionHeader;
		packet.size = sizeof(IdRolePacket);
		packet.id = static_cast<uint8_t>(c_id);
		packet.role = static_cast<uint8_t>(g_users[c_id].getRole());
		g_users[c_id].do_send_packet(&packet);

		for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i) {
			uint8_t otherId = g_rooms[room_id].player_ids[i];
			if (otherId == UINT8_MAX || otherId == c_id)
				continue;
			// 다른 유저들에게 내 정보 전송
			packet.id = static_cast<uint8_t>(c_id);
			packet.role = static_cast<uint8_t>(g_users[c_id].getRole());
			g_users[otherId].do_send_packet(&packet);

			// 나에게 다른 유저들 전송
			packet.id = otherId;
			packet.role = static_cast<uint8_t>(g_users[otherId].getRole());
			g_users[c_id].do_send_packet(&packet);
		}
		//client_id++;
		break;
	}
	case ritualHeader: 
	{
		RitualPacket packet;
		packet.header = ritualHeader;
		packet.size = sizeof(RitualPacket);

		packet.Loc1 = kPredefinedLocations[altar_locs[g_users[c_id].room_id][0]];
		packet.Loc2 = kPredefinedLocations[altar_locs[g_users[c_id].room_id][1]];
		packet.Loc3 = kPredefinedLocations[altar_locs[g_users[c_id].room_id][2]];

		g_users[c_id].do_send_packet(&packet);
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
			sess.room_id = -1;
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
		}
	}
}

void InitializeAltarLoc(int room_num) {
	if (room_num < 0 || room_num >= 100)
		return;

	for (int i = 0; i < 5; ++i)
	{
		altar_locs[room_num][i] = i;
	}

	for (int i = 0; i < 4; ++i)
	{
	    int j = i + rand() % (5 - i);
	    std::swap(altar_locs[room_num][i], altar_locs[room_num][j]);
	}
}

int main()
{
	HANDLE h_iocp;
	std::wcout.imbue(std::locale("korean"));	// 한국어로 출력

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
	/*if (!InitializeDB()) {
	std::cout << "DB 초기화 실패, 프로그램 종료\n";
	return 0;
	}*/

	for (int i = 0; i < 100; ++i) {
		g_rooms[i].room_id = i;
		InitializeAltarLoc(i);
	}
	g_h_iocp = h_iocp;
	g_db_worker = std::thread(DBWorkerLoop);

	mainLoop(h_iocp);

	//CommandThread.join();
	//StopAIWorker();
	g_db_worker_run.store(false);
	g_db_cv.notify_all();
	if (g_db_worker.joinable()) 
		g_db_worker.join();
	closesocket(g_s_socket);
	WSACleanup();
}
