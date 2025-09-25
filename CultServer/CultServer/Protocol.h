#pragma once


#include <iostream>
#include <WS2tcpip.h>
#include <unordered_map>
#include <atomic>
#include <array>
#include <unordered_set>
#include "error.h"

constexpr short SERVER_PORT = 7777;
constexpr int BUF_SIZE = 200;
constexpr int MAX_PLAYERS_PER_ROOM = 5;
constexpr int MAX_CULTIST_PER_ROOM = 4;
constexpr int MAX_POLICE_PER_ROOM = 1;

constexpr float baseX = 110.f;
constexpr float baseY = -1100.f;
constexpr float baseZ = 2770.f;

//-- ingame header
constexpr char cultistHeader = 0;
constexpr char skillHeader = 1;
constexpr char policeHeader = 2;
constexpr char particleHeader = 3;
constexpr char hitHeader = 4;
constexpr char connectionHeader = 5;
constexpr char DisconnectionHeader = 6;
constexpr char disableHeader = 7;
constexpr char disappearHeader = 17;
constexpr char appearHeader = 18;

//-- room header
constexpr char requestHeader = 8;
constexpr char enterHeader = 9;
constexpr char leaveHeader = 10;
constexpr char readyHeader = 11;
constexpr char gameStartHeader = 12;
constexpr char ritualHeader = 13;
constexpr char loginHeader = 14;
constexpr char idExistHeader = 15;
constexpr char signUpHeader = 16;

constexpr char ST_FREE{ 0 };
constexpr char ST_READY{ 1 };
constexpr char ST_INGAME{ 2 };
constexpr char ST_DISABLE{ 3 };
// player가 ST_FREE로 입장
// 방에 들어가서 ready버튼 누르면 ST_READY
// 게임에 들어가면 ST_INGAME으로 바꾸면서 room도 is Ingame

#pragma pack(push, 1)

struct room {
	uint8_t room_id;
	uint8_t police = 0;
	uint8_t cultist = 0;
	bool isIngame = false;
	uint8_t  player_ids[MAX_PLAYERS_PER_ROOM] = {
		UINT8_MAX, UINT8_MAX, UINT8_MAX,
		UINT8_MAX, UINT8_MAX
	};
};

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND };

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
	uint8_t bIsPakour;
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
	uint8_t AttackerID;
	uint8_t TargetID;
	EWeaponType Weapon;
};

struct CultistPacket {
	uint8_t header;
	uint8_t size;
	FCultistCharacterState state;
};

struct PolicePacket {
	uint8_t  header;
	uint8_t size;
	FPoliceCharacterState state;
};

struct ParticlePacket {
	uint8_t header;
	uint8_t size;
	FImpactPacket data;
};

struct HitPacket {
	uint8_t  header;
	uint8_t  size;
	FHitPacket data;
};

struct IdOnlyPacket {
	uint8_t header;
	uint8_t size;
	uint8_t id;
};

struct RoleOnlyPacket {
	uint8_t header;
	uint8_t size;
	uint8_t role;
};

struct IdRolePacket {
	uint8_t header;
	uint8_t size;
	uint8_t id;
	uint8_t role;
};

struct RoomDataPacket {
	uint8_t header;
	uint8_t size;
	room room_data;
};

struct RoomsPakcet {
	uint8_t header;
	uint8_t size;
	room rooms[10];
};

struct RoomNumberPacket {
	uint8_t header;
	uint8_t size;
	uint8_t room_number;
};

struct NoticePacket {
	uint8_t header;
	uint8_t size;
};

struct FVector {
	double x;
	double y;
	double z;
};

struct FRotator {
	double pitch;
	double yaw;
	double roll;
};

struct SkillPacket {
	uint8_t header;
	uint8_t size;
	FVector SpawnLoc;
	FRotator SpawnRot;
	uint8_t skill;
};

struct RitualPacket {
	uint8_t header;
	uint8_t size;
	FVector Loc1;
	FVector Loc2;
	FVector Loc3;
};	

struct IdPacket {
	uint8_t header;
	uint8_t size;
	char id[32];
};

struct IdPwPacket {
	uint8_t header;
	uint8_t size;
	char id[32];
	char pw[32];
};

struct BoolPacket {
	uint8_t header;
	uint8_t size;
	bool result;
	uint8_t reason;
};

#pragma pack(pop)

constexpr FVector kPredefinedLocations[5] = {
	{ -10740.0f, 10460.0f, -3124.0f },
	{ -11450.0f,  4640.0f, -3076.0f },
	{   1530.0f,  6070.0f, -3124.0f },
	{   1450.0f, -1925.0f, -3124.0f },
	{  -5730.0f,  1330.0f, -3110.0f }
};

class EXP_OVER {
public:
	EXP_OVER();

	EXP_OVER(char* packet);

	WSAOVERLAPPED	over;
	int				id;
	char			send_buffer[1024];
	WSABUF			wsabuf;
	COMP_TYPE		comp_type;
};

class SESSION {
	EXP_OVER		recv_over;
public:
	SOCKET			c_socket;
	int				id;
	int				role;
	char			state;
	int				prev_remain;
	int				room_id;
	union {
		FCultistCharacterState cultist_state;
		FPoliceCharacterState police_state;
	};
	std::unordered_set<int> visible_ids;

	void do_recv();

public:
	SESSION();
	SESSION(int );			// ai
	SESSION(int , SOCKET );	// player
	~SESSION();

	void do_send_packet(void* packet);

	void do_send(char , int , char* );

	void do_send_data(int , const void* , size_t );

	void do_send_connection(char , int , int );

	void do_send_disconnection(char , int );

	void setState(const char st);

	char getState() const;

	void setPoliceState(const FPoliceCharacterState& );

	const FPoliceCharacterState& getPoliceState() const;

	void setRole(const int r);

	int getRole() const;

	SOCKET getSocket() const;

	bool isValidSocket() const;

	bool isValidState() const;
};

extern FPoliceCharacterState AiState;