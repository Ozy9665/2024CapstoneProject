#pragma once


#include <iostream>
#include <WS2tcpip.h>
#include <unordered_map>
#include <atomic>
#include <array>
#include <unordered_set>
#include <chrono>
#include <vector>
#include "error.h"

constexpr short SERVER_PORT = 7777;
constexpr int BUF_SIZE = 200;
constexpr int MAX_ROOM = 100;
constexpr int MAX_ROOM_LIST = 10;
constexpr int MAX_PLAYERS_PER_ROOM = 5;
constexpr int MAX_CULTIST_PER_ROOM = 4;
constexpr int MAX_POLICE_PER_ROOM = 1;
constexpr int MAX_ID = INT_MAX;
constexpr int ALTAR_PER_ROOM = 3;

constexpr float VIEW_RANGE = 3000.0f;           // 시야 반경
constexpr float VIEW_RANGE_SQ = VIEW_RANGE * VIEW_RANGE;
constexpr float SPHERE_TRACE_RADIUS = 200.0f;
constexpr float HEAL_GAP = 100.0f;
constexpr float PI = 3.141592f;

constexpr float baseX = 110.f;
constexpr float baseY = -1100.f;
constexpr float baseZ = 2770.f;

constexpr float MAX_SPEED = 600.0f;
constexpr float MAP_BOUND_X = 20000.0f; // 맵 최대 크기에 맞게 수정
constexpr float MAP_BOUND_Y = 20000.0f;
constexpr float MAP_MIN_Z = -3100.0f;

//-- ingame header
constexpr char cultistHeader = 0;
constexpr char treeHeader = 1;
constexpr char policeHeader = 2;
constexpr char particleHeader = 3;
constexpr char hitHeader = 4;
constexpr char connectionHeader = 5;
constexpr char DisconnectionHeader = 6;
constexpr char disableHeader = 7;
constexpr char disappearHeader = 17;
constexpr char appearHeader = 18;
constexpr char tryHealHeader = 19;
constexpr char doHealHeader = 20;
constexpr char endHealHeader = 21;
constexpr char ritualStartHeader = 22;
constexpr char ritualDataHeader = 23;
constexpr char ritualEndHeader = 24;
constexpr char dogHeader = 25;
constexpr char crowSpawnHeader = 26;
constexpr char crowDataHeader = 27;
constexpr char crowDisableHeader = 28;

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
constexpr char quitHeader = 29;

constexpr char ST_FREE{ 0 };
constexpr char ST_READY{ 1 };
constexpr char ST_INGAME{ 2 };
constexpr char ST_DISABLE{ 3 };
// player가 ST_FREE로 입장
// 방에 들어가서 ready버튼 누르면 ST_READY
// 게임에 들어가면 ST_INGAME으로 바꾸면서 room도 is Ingame

// map
enum MAPTYPE { LANDMASS };

struct Vec3 {
	float x, y, z;
};

struct MapVertex {
	float x, y, z;
};

struct MapTriangle {
	int v0, v1, v2;
};

struct AABB {
	float minX, minY, minZ;
	float maxX, maxY, maxZ;
};

struct MapTri {
	MapVertex a, b, c;
};

struct Ray {
	Vec3 start;
	Vec3 dir;
};

constexpr Vec3 NewmapLandmassOffset{ -4280.f, 13000.f, -3120.f };

constexpr float REPATH_DIST{ 200.f };

// packet
#pragma pack(push, 1)
struct FVector {
	double x;
	double y;
	double z;

	FVector operator+(const FVector& other) const {
		return { x + other.x, y + other.y, z + other.z };
	}

	FVector operator*(double s) const {
		return { x * s, y * s, z * s };
	}
};

struct FRotator {
	double pitch;
	double yaw;
	double roll;
};

struct Room {
	int room_id;
	uint8_t police;
	uint8_t cultist;
	uint8_t isIngame;
	int  player_ids[MAX_PLAYERS_PER_ROOM];
};

struct Altar {
	FVector loc;
	bool isActivated;
	int id;
	int gauge;
	std::chrono::system_clock::time_point time;
};

struct Dog {
	int owner;
	FVector loc;
	FRotator rot;
	// 개 상태
	
};

struct Crow {
	int owner;
	FVector loc;
	FRotator rot;
	// 까마귀 상태
	bool is_alive;

};

enum COMP_TYPE {
	OP_ACCEPT, OP_RECV, OP_SEND,
	OP_LOGIN, OP_ID_EXIST, OP_SIGNUP,
	OP_ROOM_REQ, OP_ROOM_ENTER, OP_ROOM_GAMESTART, OP_ROOM_RITUAL, OP_ROOM_DISCONNECT
};

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
	float PositionX, PositionY, PositionZ;
	float RotationPitch, RotationYaw, RotationRoll;
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
	uint8_t ABP_DoHeal;
	uint8_t ABP_GetHeal;
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

struct HitResultPacket {
	uint8_t  header;
	uint16_t  size;
	int AttackerID;
	int TargetID;
	EWeaponType Weapon;
};

struct CultistPacket {
	uint8_t header;
	uint16_t size;
	FCultistCharacterState state;
};

struct CrowPacket {
	uint8_t  header;
	uint16_t size;
	Crow crow;
};

struct PolicePacket {
	uint8_t  header;
	uint16_t size;
	FPoliceCharacterState state;
};

struct DogPacket {
	uint8_t  header;
	uint16_t size;
	Dog dog;
};

struct ParticlePacket {
	uint8_t header;
	uint16_t size;
	FImpactPacket data;
};

struct HitPacket {
	uint8_t  header;
	uint16_t  size;
	EWeaponType Weapon;
	FVector TraceStart;
	FVector TraceDir;
};

struct IdOnlyPacket {
	uint8_t header;
	uint16_t size;
	int id;
};

struct RoleOnlyPacket {
	uint8_t header;
	uint16_t size;
	uint8_t role;
};

struct IdRolePacket {
	uint8_t header;
	uint16_t size;
	int id;
	uint8_t role;
};

struct RoomData {
	int room_id;
	uint8_t police;
	uint8_t cultist;
};

struct RoomsPakcet {
	uint8_t header;
	uint16_t  size;
	RoomData rooms[MAX_ROOM_LIST];
};

struct RoomNumberPacket {
	uint8_t header;
	uint16_t size;
	int room_number;
};

struct NoticePacket {
	uint8_t header;
	uint16_t size;
};

struct TreePacket {
	uint8_t header;
	uint16_t size;
	int casterId;
	FVector SpawnLoc;
	FRotator SpawnRot;
};

struct RitualPacket {
	uint8_t header;
	uint16_t size;
	FVector Loc[ALTAR_PER_ROOM];
};	

struct IdPacket {
	uint8_t header;
	uint16_t size;
	char id[32];
};

struct IdPwPacket {
	uint8_t header;
	uint16_t size;
	char id[32];
	char pw[32];
};

struct BoolPacket {
	uint8_t header;
	uint16_t size;
	bool result;
	uint8_t reason;
};

struct MovePacket {
	uint8_t header;
	uint16_t size;
	FVector SpawnLoc;
	FRotator SpawnRot;
	bool isHealer;
};

struct RitualNoticePacket {
	uint8_t header;
	uint16_t size;
	uint8_t ritual_id;
	uint8_t reason;
	// reason 0 -> start
	// reason 1 -> skill check suc
	// reason 2 -> skill check fail
	// reason 3 -> end
	// reason 4 -> ritial 100%
};

struct RitualGagePacket {
	uint8_t header;
	uint16_t size;
	uint8_t ritual_id;
	int gauge;
};

#pragma pack(pop)

class EXP_OVER {
public:
	EXP_OVER();

	EXP_OVER(char* packet);

	WSAOVERLAPPED	over;
	int				id;
	char			send_buffer[1024];
	WSABUF			wsabuf;
	COMP_TYPE		comp_type;
	char account_id[32]{};
};

class SESSION {
	EXP_OVER		recv_over;
public:
	SOCKET			c_socket;
	int				id;
	uint8_t			role;
	char			state;
	int				prev_remain;
	int				room_id;
	union {
		FCultistCharacterState cultist_state;
		FPoliceCharacterState police_state;
	};
	std::unordered_set<int> visible_ids;
	std::string account_id;
	int heal_gage;
	int heal_partner;
	union {
		Dog dog;
		Crow crow;
	};
	std::vector<Vec3> path;
	void do_recv();

public:
	SESSION();
	SESSION(int, uint8_t, int);		// ai
	SESSION(int , SOCKET );	// player
	~SESSION();

	void do_send_packet(void* packet);

	void setState(const char st);

	char getState() const;

	void setPoliceState(const FPoliceCharacterState& );

	const FPoliceCharacterState& getPoliceState() const;

	void setRole(const uint8_t r);

	uint8_t getRole() const;

	SOCKET getSocket() const;

	bool isValidSocket() const;

	bool isValidState() const;
};

constexpr FCultistCharacterState CultistDummyState{ -1, -10219.0f, 2560.0f, -3009.0f, 0.f, 90.f, 0.f, 0.f, 0.f, 0.f, 0.f,
	false, false, false, false, false, false, false, false, false, false, false, false };
constexpr FPoliceCharacterState PoliceDummyState{ -1,	110.f, -1100.f, 2770.f,	0.f, 90.f, 0.f,	0.f, 0.f, 0.f, 0.f,
	false, false, false, EWeaponType::Baton, false, EVaultingType::OneHandVault, false, false, false, false };

constexpr FVector kPredefinedLocations[5] = {
	{ -10740.0f, 10460.0f, -3124.0f },
	{ -11450.0f,  4640.0f, -3126.0f },
	{   1530.0f,  6070.0f, -3124.0f },
	{   1450.0f, -1925.0f, -3124.0f },
	{  -5730.0f,  1330.0f, -3110.0f }
};
