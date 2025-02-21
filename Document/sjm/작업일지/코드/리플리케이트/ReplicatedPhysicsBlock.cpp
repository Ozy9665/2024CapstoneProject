#include "ReplicatedPhysicsBlock.h"
#include "MySocketActor.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include <Kismet/GameplayStatics.h>

AReplicatedPhysicsBlock::AReplicatedPhysicsBlock()
{
    PrimaryActorTick.bCanEverTick = true;

    // 리플리케이션 활성화
    bReplicates = true;            // 네트워크 복제 활성화
    bAlwaysRelevant = true;        // 항상 클라이언트에 전송
    NetDormancy = DORM_Never;      // 절대 동면(Dormancy) 상태가 되지 않도록 설정
    SetReplicateMovement(true);

    // 블록의 Static Mesh 설정
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetSimulatePhysics(true);  // 물리 시뮬레이션 활성화
    RootComponent = MeshComp;
    BlockID = -1;
}

void AReplicatedPhysicsBlock::BeginPlay()
{
    Super::BeginPlay();
}

void AReplicatedPhysicsBlock::SetBlockID(int32 NewBlockID) {
	BlockID = NewBlockID;
}

void AReplicatedPhysicsBlock::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // `GetOwner()`가 `AMySocketActor`(서버)인지 확인
    AMySocketActor* ServerActor = Cast<AMySocketActor>(GetOwner());

    if (ServerActor && ServerActor->bIsServer) // 서버에서만 실행
    {
        FVector CurrentLocation = GetActorLocation();
        if (!CurrentLocation.Equals(LastSentLocation, 1.0f)) // 일정 거리 이상 움직였을 때만 전송
        {
            UE_LOG(LogTemp, Log, TEXT("Moving detected!"));
            LastSentLocation = CurrentLocation;
            ServerActor->SendObjectData(BlockID, CurrentLocation);
        }
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

bool AReplicatedPhysicsBlock::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
    return true; // 항상 네트워크 관련성이 있다고 판단하여 업데이트 유지
}
