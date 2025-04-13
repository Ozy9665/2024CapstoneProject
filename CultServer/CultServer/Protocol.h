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

#pragma pack(push, 1)

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

#pragma pack(pop)

class EXP_OVER {
public:
	EXP_OVER(int header, const void* data, size_t size);		// cultist

	EXP_OVER(int header, int id, char* mess);					// message

	EXP_OVER(int header, int id, int role);						// connection

	EXP_OVER(int header, int id);								// disconnection

	WSAOVERLAPPED	send_over;
	int				id;
	char			send_buffer[1024];
	WSABUF			send_wsabuf[1];
};

class SESSION {
private:
	SOCKET			c_socket;
	int				id;
	int				role;

	WSAOVERLAPPED	recv_over;
	char			recv_buffer[1024];
	WSABUF			recv_wsabuf[1];

	union {
		FCultistCharacterState cultist_state;
		FPoliceCharacterState police_state;
	};

	void do_recv();

public:
	SESSION();
	SESSION(int session_id, SOCKET s);
	~SESSION();

	void recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag);

	void do_send(char header, int id, char* mess);

	void do_send_cultist(int header, const void* data, size_t size);

	void do_send_connection(char header, int new_player_id, int role);

	void do_send_disconnection(char header, int id);
};

extern std::unordered_map<int, SESSION> g_users;
extern int client_id;
