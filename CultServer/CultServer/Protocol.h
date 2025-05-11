#pragma once


#include <iostream>
#include <WS2tcpip.h>
#include <unordered_map>
#include <atomic>
#include "error.h"

constexpr short SERVER_PORT = 7777;

constexpr int cultistHeader = 0x00;
constexpr int objectHeader = 0x01;
constexpr int policeHeader = 0x02;
constexpr int particleHeader = 0x03;
constexpr int hitHeader = 0x04;
constexpr int connectionHeader = 0x10;
constexpr int DisconnectionHeader = 0x11;
constexpr int readyHeader = 0x12;
constexpr int disableHeader = 0x13;

constexpr char ST_FREE{ 0 };
constexpr char ST_INGAME{ 1 };
constexpr char ST_CLOSE{ 2 };
constexpr char ST_DISABLE{ 3 };

void CALLBACK g_recv_callback(DWORD, DWORD, LPWSAOVERLAPPED, DWORD);
void CALLBACK g_send_callback(DWORD, DWORD, LPWSAOVERLAPPED, DWORD);

#pragma pack(push, 1)

enum EWeaponType : uint8_t
{
	Baton,
	Pistol,
	Taser
};

enum class EVaultingType : uint8_t
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
	float PositionX, PositionY, PositionZ;
	// 회전
	float RotationPitch, RotationYaw, RotationRoll;
	// 속도
	float VelocityX, VelocityY, VelocityZ, Speed;
	float CurrentHealth;
	// 상태
	uint8_t Crouch;
	uint8_t ABP_IsPerforming;
	uint8_t ABP_IsHitByAnAttack;
	uint8_t ABP_IsFrontKO;
	uint8_t ABP_IsElectric;
	uint8_t ABP_TTStun;
	uint8_t ABP_TTGetUp;
	uint8_t ABP_IsDead;
	uint8_t ABP_IsStunned;
};

struct FImpactPacket
{
	float ImpactX;
	float ImpactY;
	float ImpactZ;
	float NormalX;
	float NormalY;
	float NormalZ;
	float MuzzleX;
	float MuzzleY;
	float MuzzleZ;
	float MuzzlePitch;
	float MuzzleYaw;
	float MuzzleRoll;
};

struct FHitPacket {
	int AttackerID;
	int TargetID;
	EWeaponType Weapon;
};

#pragma pack(pop)

class EXP_OVER {
public:
	EXP_OVER(int , const void* , size_t );			// cultist

	EXP_OVER(int , int , char* );					// message

	EXP_OVER(int , int , int );						// connection

	EXP_OVER(int , int );							// disconnection

	WSAOVERLAPPED	send_over;
	int				id;
	char			send_buffer[1024];
	WSABUF			send_wsabuf[1];
};

class SESSION {
private:
	SOCKET				c_socket;
	int					id;
	int					role;
	std::atomic<char>	state;

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
	SESSION(int );			// ai
	SESSION(int , SOCKET );	// player
	~SESSION();

	void recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED p_over, DWORD flag);

	void do_send(char , int , char* );

	void do_send_data(int , const void* , size_t );

	void do_send_connection(char , int , int );

	void do_send_disconnection(char , int );

	void setState(const char st);

	char getState() const;

	void setPoliceState(const FPoliceCharacterState& );

	const FPoliceCharacterState& getPoliceState() const;

	int getRole() const;

	bool isValidSocket() const;

	bool isValidState() const;
};

extern std::unordered_map<int, SESSION> g_users;
extern std::atomic<int> client_id;
extern FPoliceCharacterState AiState;