#pragma once


#include <iostream>
#include <WS2tcpip.h>
#include <atomic>
#include <array>
#include <chrono>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <concurrent_unordered_map.h>
#include "error.h"

constexpr short SERVER_PORT = 7777;
constexpr int BUF_SIZE = 200;
constexpr int MAX_ROOM = 100;
constexpr int MAX_ROOM_LIST = 10;
constexpr int MAX_PLAYERS_PER_ROOM = 5;
constexpr int MAX_CULTIST_PER_ROOM = 4;
constexpr int MAX_POLICE_PER_ROOM = 1;
constexpr int MAX_ID = INT_MAX;
constexpr int MAX_USER = 10000;
constexpr int ALTAR_PER_ROOM = 3;
constexpr uint8_t INVALID_ROLE = 0xFF;

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

// map
enum MAPTYPE { LANDMASS, LEVEL3 };

enum DB_EVENT { EV_LOGIN, EV_EXIST, EV_SIGNUP };
struct DBTask {
	int c_id;
	DB_EVENT event_id;
	std::string id;
	std::string pw;
};

enum ROOM_EVENT { RM_REQ, RM_ENTER, RM_GAMESTART, RM_RITUAL, RM_DISCONNECT, RM_QUIT };
struct RoomTask {
	int c_id;
	ROOM_EVENT type;
	uint8_t role;
	int room_id;
};

enum EVENT_TYPE { EV_HEAL, EV_STUN };
struct TIMER_EVENT {
	int c_id;
	int target_id;
	std::chrono::system_clock::time_point wakeup_time;
	EVENT_TYPE event_id;

	constexpr bool operator < (const TIMER_EVENT& Left) const
	{
		return (wakeup_time > Left.wakeup_time);
	}
};

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
constexpr Vec3 NewmapLandmassLotate{ 0.f, 0.f, 0.f };
constexpr Vec3 Level3MapOffset{ 6840.f, 52540.f, 0.f };
constexpr Vec3 Level3MapLotate{ 90.f, 0.f, 0.f };

enum S_STATE { ST_FREE, ST_ROOM, ST_INGAME, ST_STUN, ST_DEAD };
enum AIState { Free, Patrol, Chase, Runaway, Ritual, Heal, Stun, Die };

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
	float Speed;
	bool is_barking;
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
	MAPTYPE maptype;
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

struct CultistBlackboard 
{
	AIState ai_state = AIState::Patrol;

	std::vector<Vec3> path{};
	Vec3 lastTargetPos{};
	Vec3 lastSnapPos{};
	int snapStreak{};

	int target_id = -1;

	Vec3 patrol_target{};
	bool has_patrol_target = false;

	int ritual_id{};
	float last_dist_to_target = FLT_MAX;

	int stuck_ticks{};

	Vec3 runaway_target{};
	bool has_runaway_target{};
	int runaway_ticks{};
};

struct PoliceBlackboard
{
	AIState ai_state = AIState::Patrol;

	std::vector<Vec3> path{};
	Vec3 lastTargetPos{};
	Vec3 lastSnapPos{};
	int snapStreak{};

	int target_id = -1;

	Vec3 patrol_target{};
	bool has_patrol_target = false;

	float last_dist_to_target = FLT_MAX;

	int stuck_ticks{};

	float aim_time{};
	int aim_target = -1;

	float attack_lock_time{};
};

struct DogBlackboard
{
	int target_id = -1;        // 현재 공격 대상
	Vec3 targetPos{};            // 이동 목표

	float dist_to_owner{};       // 경찰 거리
	float dist_to_target{};      // 적 거리

	Vec3 lastTargetPos{};
	std::vector<Vec3> path{};

	float repath_timer{};

	bool bNeedFollowOwner = false;
	bool bStopChaseForOwnerDist = false;
};

class SESSION;

class AIController {
public:
	SESSION* owner;

	AIController(SESSION* o) : owner(o) {}

	virtual void Update(float dt) = 0;
};
class CultistAIController;
class PoliceAIController;

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
	std::mutex		s_lock;
	S_STATE		state;
	
	int				prev_remain;
	int				room_id;
	union {
		FCultistCharacterState	cultist_state;
		FPoliceCharacterState	police_state;
	};
	std::unordered_set<int> visible_ids;
	std::string				account_id;
	int						heal_gauge;
	int						heal_partner;
	union {
		Dog		dog;
		Crow	crow;
	};
	void do_recv();

	// ai
	std::shared_ptr<AIController> ai;

public:
	SESSION();
	SESSION(int, uint8_t, int);		// ai
	SESSION(int , SOCKET );	// player

	void do_send_packet(void* packet);

	void setState(const S_STATE st);

	void setPoliceState(const FPoliceCharacterState& );

	const FPoliceCharacterState& getPoliceState() const;

	void setRole(const uint8_t r);

	bool isValidSocket() const;

	void setAIState(const AIState st);

	bool isAI() const;

	void resetForReuse();
};

extern std::array<std::pair<Room, MAPTYPE>, MAX_ROOM> g_rooms;
extern concurrency::concurrent_unordered_map<int, std::shared_ptr<SESSION>> g_users;

template <typename PacketT>
void broadcast_in_room(const SESSION& sender, const PacketT* packet, float view_range = -1.0f) {
	int room_id = sender.room_id;
	if (room_id < 0 || room_id >= MAX_ROOM)
		return;
	const auto& room = g_rooms[room_id].first;

	for (int pid : room.player_ids)
	{
		if (pid == -1 || pid == sender.id)
			continue;

		auto it = g_users.find(pid);
		if (it == g_users.end())
			continue;

		auto& target = it->second;

		if (!target->isValidSocket() || target->state == ST_FREE)
			continue;

		/*
		float view_range_sq = (view_range > 0) ? view_range * view_range : -1.0f;	// -1이면 전원에게 전송
		bool inRange = (view_range_sq < 0) ||
			(distanceSq(sender, *target) <= view_range_sq);

		if (inRange)
		{
			if (target->visible_ids.insert(sender_id).second)
			{
				IdOnlyPacket appear;
				appear.header = appearHeader;
				appear.size = sizeof(IdOnlyPacket);
				appear.id = sender_id;
				target->do_send_packet(&appear);
			}

			target->do_send_packet(reinterpret_cast<void*>(const_cast<PacketT*>(packet)));
		}
		else
		{
			if (target->visible_ids.erase(sender_id) > 0)
			{
				IdOnlyPacket disappear;
				disappear.header = disappearHeader;
				disappear.size = sizeof(IdOnlyPacket);
				disappear.id = sender_id;
				target->do_send_packet(&disappear);
			}
		}
		*/

		target->do_send_packet(reinterpret_cast<void*>(const_cast<PacketT*>(packet)));
	}
}

constexpr FVector LandmassSpawnLocation{ -10219.0f, 2560.0f, -3009.0f };
constexpr FVector Level3SpawnLocation{ 5440.f, 51000.f, 90.f };

constexpr FCultistCharacterState CultistDummyState{ -1, 0.f, 0.f, 0.f, 0.f, 90.f, 0.f, 0.f, 0.f, 0.f, 100.f,
	false, false, false, false, false, false, false, false, false, false, false, false };
constexpr FPoliceCharacterState PoliceDummyState{ -1, 0.f, 0.f, 0.f,	0.f, 90.f, 0.f,	0.f, 0.f, 0.f, 0.f,
	false, false, false, EWeaponType::Baton, false, EVaultingType::OneHandVault, false, false, false, false };

constexpr FVector LandMassAltarLocations[5] = {
	{ -10740.0f, 10460.0f, -3124.0f },
	{ -11450.0f,  4640.0f, -3126.0f },
	{   1530.0f,  6070.0f, -3124.0f },
	{   1450.0f, -1925.0f, -3124.0f },
	{  -5730.0f,  1330.0f, -3110.0f }
};

constexpr double BATON_RANGE{ 200.0 };
constexpr double TASER_RANGE{ 1000.0 };
constexpr double PISTOL_RANGE{ 5000.0 };
constexpr float POLICE_HALF_HEIGHT{ 90.f };

// ai
constexpr float RAD_TO_DEG{ 180.f / PI };
constexpr float DEG_TO_RAD{ PI / 180.f };
constexpr float fixed_dt{ 1.0f / 60.0f };
constexpr float pushDist{ 120.f };
constexpr float REPATH_DIST{ 200.f };
constexpr int sampleCount{ 20 };
constexpr float sampleRadius{ 2000.f };
constexpr float ALTAR_TRIGGER_RANGE{ 600.f };
constexpr float ALTAR_TRIGGER_RANGE_SQ{ ALTAR_TRIGGER_RANGE * ALTAR_TRIGGER_RANGE };
constexpr float CHASE_START_RANGE{ 1500.f };
constexpr float CHASE_STOP_RANGE{ 150.f };
constexpr float ARRIVE_RANGE{ 100.f };
constexpr float STUCK_RANGE{ 5.f };
constexpr float FOLLOW_MAX_DIST{ 800.f };
constexpr float DOG_MAX_DIST{ 3000.f };
constexpr float DOG_SPEED{ 500.f };
constexpr float POLICE_SPEED{ 350.f };
constexpr float CULTIST_SPEED{ 300.f };
constexpr float ATTACK_COOL_DOWN{ 3.f };
