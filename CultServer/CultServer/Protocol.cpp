#include "Protocol.h"

std::unordered_map<int, SESSION> g_users;
int client_id = 0;

void CALLBACK g_send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag)
{
	EXP_OVER* o = reinterpret_cast<EXP_OVER*>(p_over);
	delete o;
}

void CALLBACK g_recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag)
{
	auto my_id = reinterpret_cast<long long>(p_over->hEvent);
	g_users[my_id].recv_callback(err, num_bytes, p_over, flag);
}

EXP_OVER::EXP_OVER(int header, const void* data, size_t size) 			// player
{
	ZeroMemory(&send_over, sizeof(send_over));
	auto packet_size = 2 + size;
	if (packet_size > sizeof(send_buffer)) {
		std::cout << "PACKET TOO LARGE\n";
		exit(-1);
	}
	send_buffer[0] = static_cast<unsigned char>(header);
	send_buffer[1] = static_cast<unsigned char>(packet_size);
	memcpy(send_buffer + 2, data, size);
	send_wsabuf[0].buf = send_buffer;
	send_wsabuf[0].len = static_cast<ULONG>(packet_size);
}

EXP_OVER::EXP_OVER(int header, int id, char* mess) : id(id)				// message
{
	ZeroMemory(&send_over, sizeof(send_over));
	auto  packet_size = 3 + strlen(mess);
	if (packet_size > 255) {
		std::cout << "MESSAGE TOO LONG";
		exit(-1);
	}
	send_buffer[0] = static_cast<unsigned char>(header);
	send_buffer[1] = static_cast<unsigned char>(packet_size);
	send_buffer[2] = static_cast<unsigned char>(id);
	send_wsabuf[0].buf = send_buffer;
	send_wsabuf[0].len = static_cast<ULONG>(packet_size);
	strcpy_s(send_buffer + 3, sizeof(send_buffer) - 3, mess);
}

EXP_OVER::EXP_OVER(int header, int id, int role) : id(id)				// connection
{				
	ZeroMemory(&send_over, sizeof(send_over));

	auto packet_size = 4;
	send_buffer[0] = static_cast<unsigned char>(header);
	send_buffer[1] = static_cast<unsigned char>(packet_size);
	send_buffer[2] = static_cast<unsigned char>(id);
	send_buffer[3] = static_cast<unsigned char>(role);
	send_wsabuf[0].buf = send_buffer;
	send_wsabuf[0].len = static_cast<ULONG>(packet_size);
}

EXP_OVER::EXP_OVER(int header, int id) : id(id)							// disconnection
{
	ZeroMemory(&send_over, sizeof(send_over));

	auto packet_size = 3;
	send_buffer[0] = static_cast<unsigned char>(header);
	send_buffer[1] = static_cast<unsigned char>(packet_size);
	send_buffer[2] = static_cast<unsigned char>(id);
	send_wsabuf[0].buf = send_buffer;
	send_wsabuf[0].len = static_cast<ULONG>(packet_size);
}

void SESSION::do_recv() 
{
	DWORD recv_flag = 0;
	ZeroMemory(&recv_over, sizeof(recv_over));
	recv_over.hEvent = reinterpret_cast<HANDLE>(id);
	auto ret = WSARecv(c_socket, recv_wsabuf, 1, NULL, &recv_flag, &recv_over, g_recv_callback);
	if (0 != ret) {
		auto err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no) {
			print_error_message(err_no);
			exit(-1);
		}
	}
}

SESSION::SESSION() {
	std::cout << "DEFAULT SESSION CONSTRUCTOR CALLED!!\n";
	exit(-1);
}

SESSION::SESSION(int session_id, SOCKET s) : id(session_id), c_socket(s)
{
	recv_wsabuf[0].len = sizeof(recv_buffer);
	recv_wsabuf[0].buf = recv_buffer;

	recv_over.hEvent = reinterpret_cast<HANDLE>(session_id);

	do_recv();
}

SESSION::~SESSION()
{
	closesocket(c_socket);
}

void SESSION::recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag)
{
	if (0 != err || 0 == num_bytes) {
		std::cout << "Client [" << id << "] disconnected." << std::endl;
		for (auto& u : g_users) {
			u.second.do_send_disconnection(DisconnectionHeader, id);
		}
		g_users.erase(id);
		return;
	}
	recv_buffer[num_bytes] = 0;

	switch (recv_buffer[0]) {
	case cultistHeader:
	{
		if (num_bytes < 3 + sizeof(FCultistCharacterState)) {
			std::cout << "Invalid cultist packet size\n";
			break;
		}

		FCultistCharacterState recvState;
		memcpy(&recvState, recv_buffer + 3, sizeof(FCultistCharacterState));

		// 세션 상태에 저장
		cultist_state = recvState;
		cultist_state.PlayerID = id;

		/*std::cout << "[Cultist] ID=" << id << "\n";
		std::cout << "  States   : "
			<< "Crouching=" << recvState.bIsCrouching << ", "
			<< "PerformingRitual=" << recvState.bIsPerformingRitual << ", "
			<< "HitByAttack=" << recvState.bIsHitByAnAttack << ", "
			<< "Health=" << recvState.CurrentHealth << "\n";*/

		for (auto& u : g_users) {
			if (u.first != id) {
				u.second.do_send_data(cultistHeader, &cultist_state, sizeof(FCultistCharacterState));
			}
		}
		break;
	}
	case policeHeader: 
	{
		if (num_bytes < 3 + sizeof(FPoliceCharacterState)) {
			std::cout << "Invalid police packet size\n";
			break;
		}

		FPoliceCharacterState recvState;
		memcpy(&recvState, recv_buffer + 3, sizeof(FPoliceCharacterState));

		// 세션 상태에 저장
		police_state = recvState;
		police_state.PlayerID = id;

		for (auto& u : g_users) {
			if (u.first != id) {
				u.second.do_send_data(policeHeader, &police_state, sizeof(FPoliceCharacterState));
			}
		}
		break;
	}
	case particleHeader:
	{
		if (num_bytes < 3 + sizeof(FImpactPacket)) {
			std::cout << "Invalid particle packet size\n";
			break;
		}

		FImpactPacket impact;
		memcpy(&impact, recv_buffer + 3, sizeof(FImpactPacket));

		for (auto& u : g_users) {
			if (u.first != id) {
				u.second.do_send_data(particleHeader, &impact, sizeof(FImpactPacket));
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
		// 접속중인 유저에게 새 접속을 전송
		for (auto& u : g_users) {
			u.second.do_send_connection(connectionHeader, id, role);
		}
		// 새로 접속한 유저에게 기존 유저들을 전송
		for (auto& u : g_users) {
			if (u.first != id) {
				g_users[id].do_send_connection(connectionHeader, u.first, u.second.role);
			}	
		}

		client_id++;
		break;
	}
	default:
		std::cout << "Unknown packet type received: " << static_cast<int>(recv_buffer[0]) << std::endl;
	}

	do_recv();
}

void SESSION::do_send(char header, int id, char* mess)
{
	EXP_OVER* o = new EXP_OVER(header, id, mess);/*
	std::cout << "[Send to Client " << id << "] id: " << id
		<< ", data: " << mess[0] << mess[1] << std::endl;*/
	DWORD size_sent;
	WSASend(c_socket, o->send_wsabuf, 1, &size_sent, 0, &(o->send_over), g_send_callback);
}

void SESSION::do_send_data(int header, const void* data, size_t size)
{
	EXP_OVER* o = new EXP_OVER(header, data, size);
	DWORD size_sent;
	WSASend(c_socket, o->send_wsabuf, 1, &size_sent, 0, &(o->send_over), g_send_callback);
}

void SESSION::do_send_connection(char header, int new_player_id, int role) 
{
	EXP_OVER* o = new EXP_OVER(header, new_player_id, role);
	DWORD size_sent;
	WSASend(c_socket, o->send_wsabuf, 1, &size_sent, 0, &(o->send_over), g_send_callback);
}

void SESSION::do_send_disconnection(char header, int id) 
{
	EXP_OVER* o = new EXP_OVER(header, id);
	DWORD size_sent;
	std::cout << "disconnetion sended\n";
	WSASend(c_socket, o->send_wsabuf, 1, &size_sent, 0, &(o->send_over), g_send_callback);
}
