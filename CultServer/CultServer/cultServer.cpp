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

void CommandWorker()
{
	while (true)
	{
		std::string command;
		std::getline(std::cin, command);

		if (command == "add_ai")
		{
			int ai_id = client_id++;
			InitializeAISession(ai_id);
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
		break;
	}
	case particleHeader:
	{
		auto* p = reinterpret_cast<ParticlePacket*>(packet);
		if (p->size != sizeof(ParticlePacket)) {
			std::cout << "Invalid ParticlePacket size\n";
			break;
		}

		for (auto& [id, u] : g_users) {
			if (id != c_id && u.isValidSocket()) {
				u.do_send_data(particleHeader, p, sizeof(ParticlePacket));
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

		for (auto& [id, u] : g_users) {
			if (id != c_id && u.isValidSocket()) {
				u.do_send_data(particleHeader, p, sizeof(HitPacket));
			}
		}
		break;
	}
	case connectionHeader:
	{
		if (num_bytes < 3) {
			std::cout << "Invalid connection packet size\n";
			break;
		}

		int role = static_cast<unsigned char>(recv_buffer[2]);
		this->role = role;

		std::cout << "Client[" << id << "] connected with role: " << (role == 0 ? "Cultist" : "Police") << std::endl;
		// 1. 새로 접속한 유저(id)에게 본인 id와 role 확정 send
		g_users[id].do_send_connection(connectionHeader, id, role);
		// 2. 새로 접속한 유저(id)에게 기존 유저들 send
		for (auto& u : g_users)
		{
			if ((u.first != id))
			{
				g_users[id].do_send_connection(connectionHeader, u.first, u.second.role);
			}
		}
		// 3. 기존 유저들에게 새로 접속한 유저(id) send
		for (auto& u : g_users)
		{
			if (u.first != id && u.second.isValidSocket())
			{
				u.second.do_send_connection(connectionHeader, id, role);
			}
		}

		client_id++;
		break;
	}
	case readyHeader:
	{
		this->setState(ST_INGAME);
		break;
	}
	case disableHeader:
	{
		this->setState(ST_DISABLE);

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

			// TODO: 경찰 승리 처리 or 게임 종료 로직 추가
			for (auto& [id, session] : g_users)
			{
				if (session.isValidSocket())
				{
					session.setState(ST_FREE);
					session.do_send_disconnection(DisconnectionHeader, id);
					closesocket(c_socket);
					std::cout << "소켓 수동 종료. ID: " << id << std::endl;
					c_socket = INVALID_SOCKET;
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 전송 여유 시간

			g_users.clear(); // 서버 상태 리셋
		}
		break;
	}
	}
}

void mainLoop(HANDLE h_iocp) {
	while (true) {
		DWORD io_size;
		WSAOVERLAPPED* o = nullptr;
		ULONG_PTR key;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &io_size, &key, &o, INFINITE);
		EXP_OVER* eo = reinterpret_cast<EXP_OVER*>(o);
		if (FALSE == ret) {
			if (eo->comp_type == OP_ACCEPT)
				std::cout << "Accept Error";
			else {
				auto err_no = WSAGetLastError();
				print_error_message(err_no);
				if (g_users.count(key) != 0) {
					g_users.erase(key);
				}
			}
		}
		if ((eo->comp_type == OP_RECV || eo->comp_type == OP_SEND) && io_size == 0) {
			if (g_users.count(key) != 0) {
				g_users.erase(key);
				delete eo;
				continue;
			}
		}
		switch (eo->comp_type)
		{
		case OP_ACCEPT:
		{
			int new_id = client_id++;

			CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket), h_iocp, new_id, 0);
			g_users.emplace(new_id, SESSION(new_id, g_c_socket));

			SOCKADDR_IN* client_addr = (SOCKADDR_IN*)(g_a_over.send_buffer + sizeof(SOCKADDR_IN) + 16);
			std::cout << "클라이언트 접속: IP = " << inet_ntoa(client_addr->sin_addr) << ", Port = " << ntohs(client_addr->sin_port) << std::endl;

			ZeroMemory(&g_a_over.over, sizeof(g_a_over.over));
			int addr_size = sizeof(SOCKADDR_IN);
			g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			AcceptEx(g_s_socket, g_c_socket, g_a_over.send_buffer, 0, addr_size + 16, addr_size + 16, 0, &g_a_over.over);
			break;
		}
		case OP_RECV: {
			int remain_data = io_size + g_users[key].prev_remain;
			char* p = eo->send_buffer;
			while (remain_data > 0) {
				int packet_size = p[0];
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
	std::wcout.imbue(std::locale("korean"));	// 한국어로 출력

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);

	std::thread CommandThread(CommandWorker);

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

	CommandThread.join();
	StopAIWorker();
	closesocket(g_s_socket);
	WSACleanup();
}
