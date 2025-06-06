#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PoliceCharacter.h"
#include <array>
#include "MySocketActor.generated.h"


USTRUCT(BlueprintType)
struct Froom {
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) uint8 room_id;
	UPROPERTY(BlueprintReadWrite) uint8 police;
	UPROPERTY(BlueprintReadWrite) uint8 cultist;
	UPROPERTY(BlueprintReadWrite) bool isIngame;
	UPROPERTY(BlueprintReadWrite) TArray<int32> player_ids;
};

#pragma pack(push, 1)
struct FPoliceCharacterState
{
	int32 PlayerID;
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
	int32 PlayerID;
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

struct PacketRoom {
	uint8_t room_id;
	uint8_t police;
	uint8_t cultist;
	bool isIngame;
	uint8_t player_ids[5];
};

struct RoomsPakcet {
	uint8_t header;
	uint8_t size;
	PacketRoom rooms[10];
};

struct RoomDataPacket {
	uint8_t header;
	uint8_t size;
	PacketRoom room_data;
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

struct SkillPacket {
	uint8_t header;
	uint8_t size;
	FVector SpawnLoc;
	FRotator SpawnRot;
};

struct RitualPacket {
	uint8_t header;
	uint8_t size;
	FVector Loc1;
	FVector Loc2;
	FVector Loc3;
};

#pragma pack(pop)

UCLASS()
class CULT_API AMySocketActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMySocketActor();

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
constexpr char skillHeader = 1;
constexpr char policeHeader = 2;
constexpr char particleHeader = 3;
constexpr char hitHeader = 4;
constexpr char connectionHeader = 5;
constexpr char DisconnectionHeader = 6;
constexpr char disableHeader = 7;
//-- room header
constexpr char requestHeader = 8;
constexpr char enterHeader = 9;
constexpr char leaveHeader = 10;
constexpr char readyHeader = 11;
constexpr char gameStartHeader = 12;
constexpr char ritualHeader = 13;

constexpr FCultistCharacterState CultistDummyState{ -1, 110, -1100,  2770, 0, 90, 0 };
constexpr FPoliceCharacterState PoliceDummyState{ -1,	110.f, -1100.f, 2770.f,	0.f, 90.f, 0.f,	0.f, 0.f, 0.f, 0.f,
	false, false, false, EWeaponType::Baton, false, EVaultingType::OneHandVault, false, false, false, false };

constexpr float BatonAttackDamage = 50.0f;
constexpr float PistolAttackDamage = 50.0f;
