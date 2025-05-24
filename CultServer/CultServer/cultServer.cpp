#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <array>
#include <unordered_map>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <string>
#include "Protocol.h"
#include "error.h"
#include "PoliceAI.h"

#pragma comment(lib, "MSWSock.lib")
#pragma comment (lib, "WS2_32.LIB")

SOCKET g_s_socket, g_c_socket;
EXP_OVER g_a_over;

std::atomic<int> client_id = 0;
std::unordered_map<int, SESSION> g_users;
std::array<room, 100> g_rooms;

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
	closesocket(g_users[c_id].c_socket);
	g_users.erase(c_id);
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
		g_users[c_id].cultist_state = p->state;

		for (auto& [id, session] : g_users) {
			if (id != c_id && session.isValidSocket()) {
				session.do_send_packet(p);
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
		g_users[c_id].police_state = p->state;

		for (auto& [id, session] : g_users) {
			if (id != c_id && session.isValidSocket()) {
				session.do_send_packet(p);
			}
		}
		break;
	}
	case particleHeader:
	{
		auto* p = reinterpret_cast<ParticlePacket*>(packet);
		if (p->size != sizeof(ParticlePacket)) {
			std::cout << "Invalid ParticlePacket size\n";
			break;
		}

		for (auto& [id, session] : g_users) {
			if (id != c_id && session.isValidSocket()) {
				session.do_send_packet(p);
			}
		}
		break;
	}
	case hitHeader:
	{
		auto* p = reinterpret_cast<HitPacket*>(packet);
		if (p->size != sizeof(HitPacket)) {
			std::cout << "Invalid HitPacket size\n";
			break;
		}

		for (auto& [id, session] : g_users) {
			if (id != c_id && session.isValidSocket()) {
				session.do_send_packet(p);
			}
		}
		break;
	}
	case connectionHeader:
	{
		auto* p = reinterpret_cast<ConnectionPacket*>(packet);
		if (p->size != sizeof(ConnectionPacket)) {
			std::cout << "Invalid ConnectionPacket size\n";
			break;
		}
		
		int role = static_cast<int>(p->role);
		g_users[c_id].setRole(role);
		p->id = c_id;

		std::cout << "Client[" << c_id << "] connected with role: " << (role == 0 ? "Cultist" : "Police") << std::endl;
		// ���� ������ ����(id)���� ���� id�� role Ȯ�� send
		g_users[c_id].do_send_packet(p);
		break;
	}
	case disableHeader:
	{
		g_users[c_id].setState(ST_DISABLE);

		bool allCultistsDisabled = true;

		for (const auto& [id, session] : g_users)
		{
			if (session.getRole() == 0 && session.getState() != ST_DISABLE) /* Cultist */
			{
				allCultistsDisabled = false;
				break;
			}
		}

		if (allCultistsDisabled)
		{
			std::cout << "[Server] All cultists are disabled!\n";
			std::cout << "[Server] Police Win!\n";

			DisconnectionPacket newPacket;
			newPacket.header = DisconnectionHeader;
			newPacket.size = sizeof(DisconnectionPacket);

			// TODO: ���� �¸� ó�� or ���� ���� ���� �߰�
			std::vector<int> ids_to_disconnect;
			for (auto& [id, session] : g_users)
			{
				if (session.isValidSocket())
				{
					newPacket.id = id;
					session.do_send_packet(&newPacket);
					ids_to_disconnect.push_back(id);
				}
			}

			for (int id : ids_to_disconnect)
			{
				disconnect(id);
			}
		}
		break;
	}
	case requestHeader: 
	{ 
		// 1. Ŭ���̾�Ʈ�� room data�� request����.
		std::cout << "requestHeader recved" << std::endl;
		auto* p = reinterpret_cast<RequestPacket*>(packet);
		if (p->size != sizeof(RequestPacket)) {
			std::cout << "Invalid requestPacket size\n";
			break;
		}
		int role = static_cast<int>(p->role);
		// 2. �տ������� ingame�� false�̰�, 
		//	  g_users[c_id].role�� �÷��̾ full�� �ƴ� room�� RoomdataPakcet�� 10�� ����
		RoomdataPakcet packet;
		packet.header = requestHeader;
		packet.size = sizeof(RoomdataPakcet);

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
		// 1. room number�� ���� ��������� InRoomPacket ����
		// 1. ������Ʈ�Ǿ� ���� ���� �Ұ����ϸ� leaveHeader ����
		// 2. Ŭ�󿡼��� ���� �������ִ� InRoomPacket�� ������� room ������Ʈ
		// 3. ���� �ߴٸ�, �� room�� ������ ������Ʈ.
		// 4. ����(POLICE/CULTIST) ���� room.police �Ǵ� room.cultist++
		// 5. ���� �� ���� ��Ŷ ����
		break;
	}
	case leaveHeader: 
	{
		// 1. �濡�� �������ϱ� page�� 0�� RoomdataPakcet ����.
		// 2. room.player_ids���� �ش� id ���� (=-1)
		// 3. ���ҿ� ���� room.police �Ǵ� cultist--
		// 4. isIngame = false;
		break;
	}
	case readyHeader:
	{
		// 1. player ���¸� ready�� ����
		// 2. InRoomPacket���� �� ���� ������Ʈ�� �� �������� broadcast
		g_users[c_id].setState(ST_READY);
		break;
	}
	case gameStartHeader: 
	{
		std::cout << "gameStartHeader recved" << std::endl;
		auto* p = reinterpret_cast<EnterPacket*>(packet);
		if (p->size != sizeof(EnterPacket)) {
			std::cout << "Invalid requestPacket size\n";
			break;
		}
		int room_id = p->room_number;
		// 1. room�� isIngame�� true��
		g_rooms[room_id].isIngame = true;
		// 2. room�� ��� ����(id)���� ���� ������ broadcast
		ConnectionPacket packet;
		packet.header = connectionHeader;
		packet.size = sizeof(ConnectionPacket);
		for (int i = 0; i < MAX_PLAYERS_PER_ROOM; ++i) {
			uint8_t targetId = g_rooms[room_id].player_ids[i];
			if (targetId == UINT8_MAX || targetId == c_id)
				continue;

			for (int j = 0; j < MAX_PLAYERS_PER_ROOM; ++j)
			{
				uint8_t otherId = g_rooms[room_id].player_ids[j];
				// invalid �Ǵ� �ڱ� �ڽ� ��ŵ
				if (otherId == UINT8_MAX || otherId == targetId)
					continue;

				packet.id = otherId;
				packet.role = static_cast<uint8_t>(g_users[targetId].getRole());
				// ���� ������ targetId �� ���ǿ�
				g_users[targetId].do_send_packet(&packet);
			}
		}
		//client_id++;
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
			std::cout << "���Ⱑ ����\n";
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
			std::cout << "���Ⱑ ����\n";
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
			g_users.insert(std::make_pair(new_id, sess));	// ���� emplace�� �ٲ� �� ������?
			CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket), h_iocp, new_id, 0);
			sess.do_recv();

			SOCKADDR_IN* client_addr = (SOCKADDR_IN*)(g_a_over.send_buffer + sizeof(SOCKADDR_IN) + 16);
			std::cout << "Ŭ���̾�Ʈ ����: IP = " << inet_ntoa(client_addr->sin_addr) << ", Port = " << ntohs(client_addr->sin_port) << std::endl;
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
			delete eo;
			break;
		}
	}
}

int main()
{
	HANDLE h_iocp;
	std::wcout.imbue(std::locale("korean"));	// �ѱ���� ���

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
	
	mainLoop(h_iocp);

	//CommandThread.join();
	//StopAIWorker();
	closesocket(g_s_socket);
	WSACleanup();
}
