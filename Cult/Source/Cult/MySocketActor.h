#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PoliceCharacter.h"
#include "MySocketActor.generated.h"

#pragma pack(push, 1)

USTRUCT(BlueprintType)
struct FPoliceCharacterState
{
	GENERATED_BODY()
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

USTRUCT(BlueprintType)
struct FCultistCharacterState
{
	GENERATED_BODY()
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
	bool bIsPerformingRitual;
	bool bIsHitByAnAttack;
	float CurrentHealth;
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

constexpr int cultistHeader = 0x00;
constexpr int objectHeader = 0x01;
constexpr int policeHeader = 0x02;
constexpr int particleHeader = 0x03;
constexpr int connectionHeader = 0x10;
constexpr int DisconnectionHeader = 0x11;
