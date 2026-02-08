#include "Protocol.h"
#include <thread>

FPoliceCharacterState AiState{ -1,
	baseX, baseY, baseZ,
	0.f, 90.f, 0.f,
	0.f, 0.f, 0.f, 0.f,
	false, false, false,
	EWeaponType::Baton,
	false,
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
	uint16_t packet_size;
	memcpy(&packet_size, packet + 1, sizeof(uint16_t));
	wsabuf.len = packet_size;
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
	WSARecv(c_socket, &recv_over.wsabuf, 1, 0, &recv_flag, &recv_over.over, 0);
}

SESSION::SESSION() {}

SESSION::SESSION(int session_id, uint8_t ai_role, int room_id) 
	: c_socket(INVALID_SOCKET), id(session_id), role(ai_role), room_id(room_id),
	prev_remain{}, state{ ST_FREE }, heal_gage{}, lastTargetPos{}, lastSnapPos{}, snapStreak{} // AI Session
{
	if (ai_role == 100)   // Cultist AI
	{
		cultist_state = CultistDummyState;
		cultist_state.PlayerID = session_id;
		std::cout << "[AI] Cultist SESSION 积己 ID=" << id << "\n";
	}
	else if (ai_role == 101)  // Police AI
	{
		police_state = PoliceDummyState;
		police_state.PlayerID = session_id;
		std::cout << "[AI] Police SESSION 积己 ID=" << id << "\n";
	}
	else
	{
		std::cout << "[AI] 舅 荐 绝绰 role: " << ai_role << "\n";
	}
}

SESSION::~SESSION(){ }

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

void SESSION::setRole(const uint8_t r) {
	if(role != r)
		role = r;
}

uint8_t SESSION::getRole() const {
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
