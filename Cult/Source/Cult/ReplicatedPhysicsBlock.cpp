#include "ReplicatedPhysicsBlock.h"
#include "MySocketActor.h"
#include "GameFramework/Actor.h"
#include <Kismet/GameplayStatics.h>

AReplicatedPhysicsBlock::AReplicatedPhysicsBlock()
{
    PrimaryActorTick.bCanEverTick = true;

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
        FTransform CurrentTransform = GetActorTransform();
        if ((!CurrentTransform.GetLocation().Equals(LastSentLocation, 0.1f) ||
            !CurrentTransform.GetRotation().Rotator().Equals(LastSentRotation, 0.1f)) && BlockID != -1)
        {
            LastSentLocation = CurrentTransform.GetLocation();
            LastSentRotation = CurrentTransform.GetRotation().Rotator();
            ServerActor->SendObjectData(BlockID, CurrentTransform);
        }
    }
}
