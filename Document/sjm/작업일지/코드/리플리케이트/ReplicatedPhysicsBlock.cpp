#include "ReplicatedPhysicsBlock.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

AReplicatedPhysicsBlock::AReplicatedPhysicsBlock()
{
    PrimaryActorTick.bCanEverTick = true;

    // 리플리케이션 활성화
    bReplicates = true;
    SetReplicateMovement(true);

    // 블록의 Static Mesh 설정
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetSimulatePhysics(true);  // 물리 시뮬레이션 활성화
    RootComponent = MeshComp;
}

void AReplicatedPhysicsBlock::BeginPlay()
{
    Super::BeginPlay();
}

void AReplicatedPhysicsBlock::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority()) // 서버에서만 실행
    {
        FVector CurrentLocation = GetActorLocation();
        FRotator CurrentRotation = GetActorRotation();
        Server_UpdatePhysics(CurrentLocation, CurrentRotation);
    }
}

// 서버에서 위치 업데이트 (RPC)
void AReplicatedPhysicsBlock::Server_UpdatePhysics_Implementation(FVector NewLocation, FRotator NewRotation)
{
    ReplicatedLocation = NewLocation;
    ReplicatedRotation = NewRotation;
}

bool AReplicatedPhysicsBlock::Server_UpdatePhysics_Validate(FVector NewLocation, FRotator NewRotation)
{
    return true;
}

// 클라이언트에서 위치/회전 보정
void AReplicatedPhysicsBlock::OnRep_UpdatePhysics()
{
    SetActorLocation(ReplicatedLocation);
    SetActorRotation(ReplicatedRotation);
}

// 리플리케이션 등록
void AReplicatedPhysicsBlock::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AReplicatedPhysicsBlock, ReplicatedLocation);
    DOREPLIFETIME(AReplicatedPhysicsBlock, ReplicatedRotation);
}
