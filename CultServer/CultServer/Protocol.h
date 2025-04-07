#pragma once

#include <iostream>
#include <WS2tcpip.h>
#include <unordered_map>
#include "error.h"

const short SERVER_PORT = 7777;

const int cultistHeader = 0x00;
const int objectHeader = 0x01;
const int policeHeader = 0x02;
const int particleHeader = 0x03;
const int connectionHeader = 0x10;
const int DisconnectionHeader = 0x11;

void CALLBACK g_recv_callback(DWORD, DWORD, LPWSAOVERLAPPED, DWORD);
void CALLBACK g_send_callback(DWORD, DWORD, LPWSAOVERLAPPED, DWORD);

enum EWeaponType
{
	Baton,
	Pistol,
	Taser
};

enum class EVaultingType
{
	OneHandVault,
	TwoHandVault,
	FrontFlip
};

struct FPoliceCharacterState
{
	int PlayerID;
	// 위치
	float PositionX;
	float PositionY;
	float PositionZ;
	// 회전
	float RotationPitch;
	float RotationYaw;
	float RotationRoll;
	// 속도
	float VelocityX;
	float VelocityY;
	float VelocityZ;
	float Speed;

	bool bIsCrouching;
	bool bIsAiming;
	bool bIsAttacking;
	EWeaponType CurrentWeapon;
	bool bIsPakour;
	bool bIsNearEnoughToPakour;
	EVaultingType CurrentVaultType;
	bool bIsOneHand;
	bool bIsTwoHand;
	bool bIsFlip;
	bool bIsShooting;
};

struct FCultistCharacterState
{
	int playerID;
	// 위치
	float PositionX;
	float PositionY;
	float PositionZ;
	// 회전
	float RotationPitch;
	float RotationYaw;
	float RotationRoll;
	// 속도
	float VelocityX;
	float VelocityY;
	float VelocityZ;
	float Speed;

	bool bIsCrouching;
	bool bIsPerformingRitual;
	bool bIsHitByAnAttack;
	float CurrentHealth;
};

class SESSION;

extern std::unordered_map<int, SESSION> g_users;

class EXP_OVER {
public:
	EXP_OVER(int header, const void* data, size_t size) {			// cultist
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

	EXP_OVER(int header, int id, char* mess) : id(id) {
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

	EXP_OVER(int header, int id) : id(id) {							// connection, disconnection
		ZeroMemory(&send_over, sizeof(send_over));

		auto packet_size = 3;
		send_buffer[0] = static_cast<unsigned char>(header);
		send_buffer[1] = static_cast<unsigned char>(packet_size);
		send_buffer[2] = static_cast<unsigned char>(id);
		send_wsabuf[0].buf = send_buffer;
		send_wsabuf[0].len = static_cast<ULONG>(packet_size);
	}

	WSAOVERLAPPED	send_over;
	int				id;
	char			send_buffer[1024];
	WSABUF			send_wsabuf[1];
};

class SESSION {
private:
	SOCKET			c_socket;
	int				id;

	WSAOVERLAPPED	recv_over;
	char			recv_buffer[1024];
	WSABUF			recv_wsabuf[1];

	// USER DATA
	FCultistCharacterState cultist_state;

	void do_recv() {
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

public:
	SESSION() {
		std::cout << "DEFAULT SESSION CONSTRUCTOR CALLED!!\n";
		exit(-1);
	}
	SESSION(int session_id, SOCKET s) : id(session_id), c_socket(s)
	{
		recv_wsabuf[0].len = sizeof(recv_buffer);
		recv_wsabuf[0].buf = recv_buffer;

		recv_over.hEvent = reinterpret_cast<HANDLE>(session_id);

		do_recv();
	}
	~SESSION()
	{
		closesocket(c_socket);
	}

	void recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag)
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
			cultist_state.playerID = id;

			/*std::cout << "[Cultist] ID=" << id << "\n";
			std::cout << "  States   : "
				<< "Crouching=" << recvState.bIsCrouching << ", "
				<< "PerformingRitual=" << recvState.bIsPerformingRitual << ", "
				<< "HitByAttack=" << recvState.bIsHitByAnAttack << ", "
				<< "Health=" << recvState.CurrentHealth << "\n";*/

			break;
		}	
		case policeHeader:
			break;
		default:
			std::cout << "Unknown packet type received: " << static_cast<int>(recv_buffer[0]) << std::endl;
		}

		for (auto& u : g_users) {
			if (u.first != id) {
				u.second.do_send_cultist(cultistHeader, &cultist_state, sizeof(FCultistCharacterState));

			}
		}

		do_recv();
	}

	void do_send(char header, int id, char* mess)
	{
		EXP_OVER* o = new EXP_OVER(header, id, mess);/*
		std::cout << "[Send to Client " << id << "] id: " << id
			<< ", data: " << mess[0] << mess[1] << std::endl;*/
		DWORD size_sent;
		WSASend(c_socket, o->send_wsabuf, 1, &size_sent, 0, &(o->send_over), g_send_callback);
	}

	void do_send_cultist(int header, const void* data, size_t size)
	{
		EXP_OVER* o = new EXP_OVER(header, data, size);
		DWORD size_sent;
		WSASend(c_socket, o->send_wsabuf, 1, &size_sent, 0, &(o->send_over), g_send_callback);
	}

	void do_send_connection(char header, int new_player_id) {
		EXP_OVER* o = new EXP_OVER(header, new_player_id);
		DWORD size_sent;
		WSASend(c_socket, o->send_wsabuf, 1, &size_sent, 0, &(o->send_over), g_send_callback);
	}

	void do_send_disconnection(char header, int id) {
		EXP_OVER* o = new EXP_OVER(header, id);
		DWORD size_sent;
		std::cout << "disconnetion sended\n";
		WSASend(c_socket, o->send_wsabuf, 1, &size_sent, 0, &(o->send_over), g_send_callback);
	}
};
