#include "Protocol.h"
#include <thread>

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
	WSARecv(c_socket, &recv_over.wsabuf, 1, 0, &recv_flag, &recv_over.over, 0);
}

SESSION::SESSION() {}

SESSION::SESSION(int session_id) : c_socket(INVALID_SOCKET), id(session_id), role(99)		// AI Session
{
	AiState.PlayerID = id;
	police_state = AiState;
	std::cout << "AI SESSION »ý¼ºµÊ. ID: " << AiState.PlayerID << " role: 99 (PoliceAI)" << std::endl;
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
