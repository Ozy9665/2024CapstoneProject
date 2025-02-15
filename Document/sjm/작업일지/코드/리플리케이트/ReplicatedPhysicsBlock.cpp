#include "ReplicatedPhysicsBlock.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

AReplicatedPhysicsBlock::AReplicatedPhysicsBlock()
{
    PrimaryActorTick.bCanEverTick = true;

    // ���ø����̼� Ȱ��ȭ
    bReplicates = true;
    SetReplicateMovement(true);

    // ����� Static Mesh ����
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetSimulatePhysics(true);  // ���� �ùķ��̼� Ȱ��ȭ
    RootComponent = MeshComp;
}

void AReplicatedPhysicsBlock::BeginPlay()
{
    Super::BeginPlay();
}

void AReplicatedPhysicsBlock::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority()) // ���������� ����
    {
        FVector CurrentLocation = GetActorLocation();
        FRotator CurrentRotation = GetActorRotation();
        Server_UpdatePhysics(CurrentLocation, CurrentRotation);
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
