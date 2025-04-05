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
	bool bIsPerformingRitual;
	bool bIsHitByAnAttack;
	float CurrentHealth;
};

class SESSION;

extern std::unordered_map<int, SESSION> g_users;

class EXP_OVER {
public:
	EXP_OVER(char header, long long id, char* mess) : id(id) {
		ZeroMemory(&send_over, sizeof(send_over));

		auto  packet_size = 3 + strlen(mess);
		if (packet_size > 255) {
			std::cout << "MESSAGE TOO LONG";
			exit(-1);
		}
		send_buffer[0] = header;
		send_buffer[1] = static_cast<unsigned char>(packet_size);
		send_buffer[2] = static_cast<unsigned char>(id);
		send_wsabuf[0].buf = send_buffer;
		send_wsabuf[0].len = static_cast<ULONG>(packet_size);
		strcpy_s(send_buffer + 3, sizeof(send_buffer) - 3, mess);
	}

	EXP_OVER(long long id, char header) : id(id) {
		ZeroMemory(&send_over, sizeof(send_over));

		auto packet_size = 3;
		send_buffer[0] = header;
		send_buffer[1] = static_cast<unsigned char>(packet_size);
		send_buffer[2] = static_cast<unsigned char>(id);
		send_wsabuf[0].buf = send_buffer;
		send_wsabuf[0].len = static_cast<ULONG>(packet_size);
	}

	WSAOVERLAPPED	send_over;
	long long		id;
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
	SESSION(long long session_id, SOCKET s) : id(session_id), c_socket(s)
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
			// 다른 모든 클라이언트에 끊김 알림 (헤더 2 사용)
			for (auto& u : g_users) {
				if (u.first != id) {
					u.second.do_send_disconnection(2, id);
				}
			}
			g_users.erase(id);
			return;
		}
		recv_buffer[num_bytes] = 0;
		std::cout << "From Client[" << id << "] : " << recv_buffer << std::endl;

		switch (recv_buffer[0]) {
		case cultistHeader:
			break;
		case policeHeader:
			break;
		default:
			std::cout << "Unknown packet type received: " << static_cast<int>(recv_buffer[0]) << std::endl;
		}

		char send_data[1024];

		for (auto& u : g_users) {
			u.second.do_send(1, id, send_data);
		}

		do_recv();
	}

	void do_send(char header, long long id, char* mess)
	{
		EXP_OVER* o = new EXP_OVER(header, id, mess);
		std::cout << "[Send to Client " << id << "] id: " << id
			<< ", data: " << mess[0] << mess[1] << std::endl;
		DWORD size_sent;
		WSASend(c_socket, o->send_wsabuf, 1, &size_sent, 0, &(o->send_over), g_send_callback);
	}

	void do_send_connection(char header) {
		EXP_OVER* o = new EXP_OVER(id, header);
		DWORD size_sent;
		WSASend(c_socket, o->send_wsabuf, 1, &size_sent, 0, &(o->send_over), g_send_callback);
	}

	void do_send_disconnection(char header, int id) {
		EXP_OVER* o = new EXP_OVER(id, header);
		DWORD size_sent;
		WSASend(c_socket, o->send_wsabuf, 1, &size_sent, 0, &(o->send_over), g_send_callback);
	}
};
