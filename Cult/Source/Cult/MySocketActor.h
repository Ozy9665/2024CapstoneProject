#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PoliceCharacter.h"
#include <array>
#include "MySocketActor.generated.h"

USTRUCT(BlueprintType)
struct Froom {
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) int room_id;
	UPROPERTY(BlueprintReadWrite) uint8 police;
	UPROPERTY(BlueprintReadWrite) uint8 cultist;
};

constexpr int MAX_PLAYERS_PER_ROOM = 5;
constexpr int MAX_ROOM_LIST = 10;

#pragma pack(push, 1)
struct FNetVec {
	double x; 
	double y; 
	double z;
};

struct FNetRot {
	double x;
	double y;
	double z;
};

struct Dog {
	int owner;
	FNetVec loc;
	FNetRot rot;
	// 개 상태

};

struct Crow {
	int owner;
	FNetVec loc;
	FNetRot rot;
	// 까마귀 상태
	bool is_alive;

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

	float CurrentHealth;
	// 상태
	bool Crouch;
	bool ABP_IsPerforming;
	bool ABP_IsHitByAnAttack;
	bool ABP_IsFrontKO;
	bool ABP_IsElectric;
	bool ABP_TTStun;
	bool ABP_TTGetUp;
	bool ABP_IsDead;
	bool ABP_IsStunned;
	bool ABP_DoHeal;
	bool ABP_GetHeal;
	bool bIsPakour;
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
	FNetVec TraceStart;
	FNetVec TraceDir;
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

struct PacketRoom {
	int room_id;
	uint8_t police;
	uint8_t cultist;
	uint8_t isIngame;
	int player_ids[5];
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

struct RoomDataPacket {
	uint8_t header;
	uint16_t size;
	PacketRoom room_data;
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
	FNetVec SpawnLoc;
	FNetRot SpawnRot;
};

struct RitualPacket {
	uint8_t header;
	uint16_t size;
	FNetVec Loc1;
	FNetVec Loc2;
	FNetVec Loc3;
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
	FNetVec SpawnLoc;
	FNetRot SpawnRot;
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

UCLASS()
class CULT_API AMySocketActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMySocketActor();
	static FVector ToUE(const FNetVec&);
	static FRotator ToUE(const FNetRot&);
	static FNetVec ToNet(const FVector&);
	static FNetRot ToNet(const FRotator&);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};

constexpr int32 BufferSize{ 1024 };
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

constexpr FCultistCharacterState CultistDummyState{ -1, -8029.0, 3099.0,  -2989.0, 0, 90, 0 };
constexpr FPoliceCharacterState PoliceDummyState{ -1,	110.f, -1100.f, 2770.f,	0.f, 90.f, 0.f,	0.f, 0.f, 0.f, 0.f,
	false, false, false, EWeaponType::Baton, false, EVaultingType::OneHandVault, false, false, false, false };

constexpr float BatonAttackDamage = 50.0f;
constexpr float PistolAttackDamage = 50.0f;
