#include "ReplicatedPhysicsBlock.h"
#include "MySocketActor.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include <Kismet/GameplayStatics.h>

AReplicatedPhysicsBlock::AReplicatedPhysicsBlock()
{
    PrimaryActorTick.bCanEverTick = true;

    // ���ø����̼� Ȱ��ȭ
    bReplicates = true;            // ��Ʈ��ũ ���� Ȱ��ȭ
    bAlwaysRelevant = true;        // �׻� Ŭ���̾�Ʈ�� ����
    NetDormancy = DORM_Never;      // ���� ����(Dormancy) ���°� ���� �ʵ��� ����
    SetReplicateMovement(true);

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
        FVector CurrentLocation = GetActorLocation();
        if (!CurrentLocation.Equals(LastSentLocation, 1.0f)) // ���� �Ÿ� �̻� �������� ���� ����
        {
            UE_LOG(LogTemp, Log, TEXT("Moving detected!"));
            LastSentLocation = CurrentLocation;
            ServerActor->SendObjectData(BlockID, CurrentLocation);
        }
    }
}

// �������� ��ġ ������Ʈ (RPC)
void AReplicatedPhysicsBlock::Server_UpdatePhysics_Implementation(FVector NewLocation, FRotator NewRotation)
{
    ReplicatedLocation = NewLocation;
    ReplicatedRotation = NewRotation;
}

bool AReplicatedPhysicsBlock::Server_UpdatePhysics_Validate(FVector NewLocation, FRotator NewRotation)
{
    return true;
}

// Ŭ���̾�Ʈ���� ��ġ/ȸ�� ����
void AReplicatedPhysicsBlock::OnRep_UpdatePhysics()
{
    SetActorLocation(ReplicatedLocation);
    SetActorRotation(ReplicatedRotation);
}

// ���ø����̼� ���
void AReplicatedPhysicsBlock::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AReplicatedPhysicsBlock, ReplicatedLocation);
    DOREPLIFETIME(AReplicatedPhysicsBlock, ReplicatedRotation);
}

bool AReplicatedPhysicsBlock::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
    return true; // �׻� ��Ʈ��ũ ���ü��� �ִٰ� �Ǵ��Ͽ� ������Ʈ ����
}
