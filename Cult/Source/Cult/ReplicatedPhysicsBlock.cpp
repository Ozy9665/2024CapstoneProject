#include "ReplicatedPhysicsBlock.h"
#include "MySocketActor.h"
#include "GameFramework/Actor.h"
#include <Kismet/GameplayStatics.h>

AReplicatedPhysicsBlock::AReplicatedPhysicsBlock()
{
    PrimaryActorTick.bCanEverTick = true;

    // ����� Static Mesh ����
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetSimulatePhysics(true);  // ���� �ùķ��̼� Ȱ��ȭ
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

    // `GetOwner()`�� `AMySocketActor`(����)���� Ȯ��
    AMySocketActor* ServerActor = Cast<AMySocketActor>(GetOwner());

    if (ServerActor && ServerActor->bIsServer) // ���������� ����
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
