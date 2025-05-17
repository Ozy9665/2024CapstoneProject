#include "Protocol.h"
#include <thread>

std::unordered_map<int, SESSION> g_users;
std::atomic<int> client_id = 0;
FPoliceCharacterState AiState{ -1,
	110.f, -1100.f, 2770.f,
	0.f, 90.f, 0.f,
	0.f, 0.f, 0.f, 0.f,
	false, false, false,
	EWeaponType::Baton,
	false, false,
	EVaultingType::OneHandVault,
	false, false, false, false
};

EXP_OVER::EXP_OVER()
{
	wsabuf.len = BUF_SIZE;
	wsabuf.buf = send_buffer;
	comp_type = OP_RECV;
	ZeroMemory(&over, sizeof(over));
}

EXP_OVER::EXP_OVER(char* packet)
{
	wsabuf.len = packet[1];
	wsabuf.buf = send_buffer;
	ZeroMemory(&over, sizeof(over));
	comp_type = OP_SEND;
	memcpy(send_buffer, packet, packet[1]);
}

void SESSION::do_recv() 
{
	if (c_socket == INVALID_SOCKET)
	{
		std::cout << "do_recv() aborted: invalid socket\n";
		return;
	}
	DWORD recv_flag = 0;
	memset(&recv_over.over, 0, sizeof(recv_over.over));
	recv_over.wsabuf.len = BUF_SIZE - prev_remain;
	recv_over.wsabuf.buf = recv_over.send_buffer + prev_remain;
	auto ret = WSARecv(c_socket, &recv_over.wsabuf, 1, 0, &recv_flag, &recv_over.over, 0);
	if (0 != ret) {
		auto err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no) {
			std::cout << "WSARecv failed! Error code: " << err_no << std::endl;
			print_error_message(err_no);
			exit(-1);
		}
	}	
}

SESSION::SESSION() {
	id = -1;
	role = -1;
	c_socket = 0;
	state = ST_FREE;
	prev_remain = 0;
}

SESSION::SESSION(int session_id) : c_socket(INVALID_SOCKET), id(session_id), role(99)		// AI Session
{
	AiState.PlayerID = id;
	police_state = AiState;
	std::cout << "AI SESSION 생성됨. ID: " << AiState.PlayerID << " role: 99 (PoliceAI)" << std::endl;
}

SESSION::SESSION(int session_id, SOCKET s) : id(session_id), c_socket(s), prev_remain(0)	// users Session
{
	if (c_socket == INVALID_SOCKET) {
		std::cout << "SESSION 생성 실패. INVALID_SOCKET\n";
		return;
	}
}

SESSION::~SESSION(){ }

//void SESSION::recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag)
//{
//	if (c_socket == INVALID_SOCKET)
//	{
//		std::cout << "[recv_callback] Skipped due to invalid socket. ID: " << id << "\n";
//		return;
//	}
//	if (0 != err || 0 == num_bytes) {
//		std::cout << "Client [" << id << "] disconnected." << std::endl;
//		for (auto& u : g_users) {
//			u.second.do_send_disconnection(DisconnectionHeader, id);
//		}
//		g_users.erase(id);
//		return;
//	}
//	recv_buffer[num_bytes] = 0;
//
//	switch (recv_buffer[0]) {
//	case cultistHeader:
//	{
//		if (num_bytes < 3 + sizeof(FCultistCharacterState)) {
//			std::cout << "Invalid cultist packet size\n";
//			break;
//		}
//
//		FCultistCharacterState recvState;
//		memcpy(&recvState, recv_buffer + 3, sizeof(FCultistCharacterState));
//
//		// 세션 상태에 저장
//		cultist_state = recvState;
//		cultist_state.PlayerID = id;
//		//std::cout << cultist_state.PositionX << " " << cultist_state.PositionY << "\n";
//		//std::cout << "[Cultist] ID=" << cultist_state.CurrentHealth << "\n";
//		//std::cout << "  States   : Crouch=" << (cultist_state.Crouch ? "true" : "false") << std::endl;
//		//std::cout << "  States   : ABP_TTStun=" << (cultist_state.ABP_TTStun ? "true" : "false") << std::endl;
//		//std::cout << "  States   : ABP_IsDead=" << (cultist_state.ABP_IsDead ? "true" : "false") << std::endl;
//
//
//		for (auto& u : g_users) {
//			if (u.first != id && g_users[id].isValidSocket()) {
//				u.second.do_send_data(cultistHeader, &cultist_state, sizeof(FCultistCharacterState));
//			}
//		}
//		break;
//	}
//	case policeHeader: 
//	{
//		if (num_bytes < 3 + sizeof(FPoliceCharacterState)) {
//			std::cout << "Invalid police packet size\n";
//			break;
//		}
//
//		FPoliceCharacterState recvState;
//		memcpy(&recvState, recv_buffer + 3, sizeof(FPoliceCharacterState));
//
//		// 세션 상태에 저장
//		police_state = recvState;
//		police_state.PlayerID = id;
//
//		for (auto& u : g_users) {
//			if (u.first != id && g_users[id].isValidSocket()) {
//				u.second.do_send_data(policeHeader, &police_state, sizeof(FPoliceCharacterState));
//			}
//		}
//		break;
//	}
//	case particleHeader:
//	{
//		if (num_bytes < 3 + sizeof(FImpactPacket)) {
//			std::cout << "Invalid particle packet size\n";
//			break;
//		}
//
//		FImpactPacket impact;
//		memcpy(&impact, recv_buffer + 3, sizeof(FImpactPacket));
//
//		for (auto& u : g_users) {
//			if (u.first != id && g_users[id].isValidSocket()) {
//				u.second.do_send_data(particleHeader, &impact, sizeof(FImpactPacket));
//			}
//		}
//		break;
//	}
//	case hitHeader:
//	{
//		if (num_bytes < 2 + sizeof(FHitPacket)) {
//			std::cout << "Invalid hit packet size\n";
//			break;
//		}
//
//		FHitPacket recvPacket;
//		memcpy(&recvPacket, recv_buffer + 2, sizeof(FHitPacket));
//		for (auto& u : g_users) {
//			if ((u.first != id) && u.second.isValidSocket()) {
//				u.second.do_send_data(hitHeader, &recvPacket, sizeof(FHitPacket));
//			}
//		}
//		break;
//	}
//	case connectionHeader:
//	{
//		if (num_bytes < 3) {
//			std::cout << "Invalid connection packet size\n";
//			break;
//		}
//
//		int role = static_cast<unsigned char>(recv_buffer[2]);
//		this->role = role;
//
//		std::cout << "Client[" << id << "] connected with role: " << (role == 0 ? "Cultist" : "Police") << std::endl;
//		// 1. 새로 접속한 유저(id)에게 본인 id와 role 확정 send
//		g_users[id].do_send_connection(connectionHeader, id, role);
//		// 2. 새로 접속한 유저(id)에게 기존 유저들 send
//		for (auto& u : g_users)
//		{
//			if ((u.first != id))
//			{
//				g_users[id].do_send_connection(connectionHeader, u.first, u.second.role);
//			}
//		}
//		// 3. 기존 유저들에게 새로 접속한 유저(id) send
//		for (auto& u : g_users)
//		{
//			if (u.first != id && u.second.isValidSocket())
//			{
//				u.second.do_send_connection(connectionHeader, id, role);
//			}
//		}
//
//		client_id++;
//		break;
//	}
//	case readyHeader:
//	{
//		this->setState(ST_INGAME);
//		break;
//	}
//	case disableHeader:
//	{
//		this->setState(ST_DISABLE);
//
//		bool allCultistsDisabled = true;
//
//		for (const auto& [id, session] : g_users)
//		{
//			if (session.getRole() == 0 && session.getState() != ST_DISABLE) /* Cultist */
//			{
//				allCultistsDisabled = false;
//				break;
//			}
//		}
//
//		if (allCultistsDisabled)
//		{
//			std::cout << "[Server] All cultists are disabled!\n";
//			std::cout << "[Server] Police Win!\n";
//
//			// TODO: 경찰 승리 처리 or 게임 종료 로직 추가
//			for (auto& [id, session] : g_users)
//			{
//				if (session.isValidSocket())
//				{
//					session.setState(ST_FREE);
//					session.do_send_disconnection(DisconnectionHeader, id);
//					closesocket(c_socket);
//					std::cout << "소켓 수동 종료. ID: " << id << std::endl;
//					c_socket = INVALID_SOCKET;
//				}
//			}
//			std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 전송 여유 시간
//
//			g_users.clear(); // 서버 상태 리셋
//		}
//		break;
//	}
//	default:
//		std::cout << "Unknown packet type received: " << static_cast<int>(recv_buffer[0]) << std::endl;
//	}
//
//	do_recv();
//}

void SESSION::do_send_packet(void* packet)
{
	EXP_OVER* packet_data = new EXP_OVER{ reinterpret_cast<char*>(packet) };
	WSASend(c_socket, &packet_data->wsabuf, 1, 0, 0, &packet_data->over, 0);
}

void SESSION::setState(const char st) {
	state = st;
}

char SESSION::getState() const {
	return state;
}

void SESSION::setPoliceState(const FPoliceCharacterState& state) {
	police_state = state;
}

const FPoliceCharacterState& SESSION::getPoliceState() const
{
	return police_state;
}

void SESSION::setRole(const int r) {
	role = r;
}

int SESSION::getRole() const {
	return role;
}

SOCKET SESSION::getSocket() const {
	return c_socket;
}

bool SESSION::isValidSocket() const
{
	return c_socket != INVALID_SOCKET;
}

bool SESSION::isValidState() const {
	return (state == ST_INGAME) || (state == ST_DISABLE);
}