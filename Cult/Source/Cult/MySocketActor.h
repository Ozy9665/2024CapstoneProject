#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PoliceCharacter.h"
#include "MySocketActor.generated.h"

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
	bool bIsNearEnoughToPakour;
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

struct CultistPacket {
	char header;
	unsigned char size;
	FCultistCharacterState state;
};

struct PolicePacket {
	char header;
	unsigned char size;
	FPoliceCharacterState state;
};

struct ParticlePacket {
	char header;
	unsigned char size;
	FImpactPacket data;
};

struct HitPacket {
	char header;
	unsigned char size;
	FHitPacket data;
};

struct ConnectionPacket {
	char header;
	unsigned char size;
	char role;
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

constexpr char cultistHeader = 0;
constexpr char objectHeader = 1;
constexpr char policeHeader = 2;
constexpr char particleHeader = 3;
constexpr char hitHeader = 4;
constexpr char connectionHeader = 5;
constexpr char DisconnectionHeader = 6;
constexpr char readyHeader = 7;
constexpr char disableHeader = 8;

constexpr FCultistCharacterState CultistDummyState{ -1, 110, -1100,  2770, 0, 90, 0 };
constexpr FPoliceCharacterState PoliceDummyState{ -1,	110.f, -1100.f, 2770.f,	0.f, 90.f, 0.f,	0.f, 0.f, 0.f, 0.f,
	false, false, false, EWeaponType::Baton, false, false, EVaultingType::OneHandVault, false, false, false, false };

constexpr float BatonAttackDamage = 50.0f;
constexpr float PistolAttackDamage = 50.0f;
