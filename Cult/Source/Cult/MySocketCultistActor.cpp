#include "MySocketCultistActor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Blueprint/AIBlueprintHelperLibrary.h>
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Camera/CameraActor.h"
#include "CultistCharacter.h"
#include "Components/TextBlock.h"
#include "TreeObstacleActor.h"
#include "ProceduralBranchActor.h"
#include "CrowActor.h"

#pragma comment(lib, "ws2_32.lib")
AMySocketCultistActor* MySocketCultistActor = nullptr;

// Sets default values
AMySocketCultistActor::AMySocketCultistActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AMySocketCultistActor::BeginPlay()
{
    Super::BeginPlay();
    MySocketCultistActor = this;
    MyCharacter = Cast<ACultistCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    if (!MyCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("My character not found!"));
    }
    GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) {
        UE_LOG(LogTemp, Error, TEXT("My Socket Cultist Actor Failed!"));
    }
}

void AMySocketCultistActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (ClientSocket != INVALID_SOCKET)
    {
        SendDisconnection();
    }
}

void AMySocketCultistActor::SetClientSocket(SOCKET InSocket, int32 RoomNumber)
{
    ClientSocket = InSocket;
    if (ClientSocket != INVALID_SOCKET)
    {
        ReceiveData();
        UE_LOG(LogTemp, Log, TEXT("Cultist Client socket set. Starting ReceiveData."));

        RoomNumberPacket packet;
        packet.header = gameStartHeader;
        packet.size = sizeof(RoomNumberPacket);
        packet.room_number = RoomNumber;
        
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&packet), sizeof(RoomNumberPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SetClientSocket failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid socket passed to SetClientSocket."));
    }
}

void AMySocketCultistActor::LogAndCleanupSocketError(const TCHAR* ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("%s with error: %ld"), ErrorMessage, WSAGetLastError());
    if (ClientSocket != INVALID_SOCKET)
    {
        closesocket(ClientSocket);
        ClientSocket = INVALID_SOCKET;
    }
    WSACleanup();
}

void AMySocketCultistActor::ReceiveData()
{
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
        {
            while (true)
            {
                char Buffer[BufferSize];
                int32 BytesReceived = recv(ClientSocket, Buffer, BufferSize, 0);
                if (BytesReceived > 0)
                {
                    int PacketType = static_cast<int>(static_cast<unsigned char>(Buffer[0]));
                    int PacketSize = static_cast<int>(static_cast<unsigned char>(Buffer[1]));
                    if (BytesReceived != PacketSize)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Invalid packet size: Received %d, Expected %d: header:%d"), BytesReceived, PacketSize, PacketType);
                        continue;
                    }

                    switch (PacketType)
                    {
                    case cultistHeader:
                        ProcessCultistData(Buffer);
                        break;
                    case treeHeader:
                        ProcessTreeData(Buffer);
                        break;
                    case crowSpawnHeader:
                        ProcessCrowSpawnData(Buffer);
                        break;
                    case crowDataHeader:
                        ProcessCrowData(Buffer);
                        break;
                    case crowDisableHeader:
                        ProcessCrowDisable(Buffer);
                        break;
                    case policeHeader:
                        ProcessPoliceData(Buffer);
                        break;
                    case dogHeader:
                        ProcessDogData(Buffer);
                        break;
                    case particleHeader:
                        ProcessParticleData(Buffer);
                        break;
                    case hitHeader:
                        ProcessHitData(Buffer);
                        break;
                    case connectionHeader:
                        ProcessConnection(Buffer);
                        break;
                    case DisconnectionHeader:
                        ProcessDisconnection(Buffer);
                        break;
                    case disappearHeader:
                    {
                        unsigned char id = static_cast<unsigned char>(Buffer[2]);
                        HideCharacter(id, true);
                        break;
                    }
                    case appearHeader:
                    {
                        unsigned char id = static_cast<unsigned char>(Buffer[2]);
                        HideCharacter(id, false);
                        break;
                    }
                    case doHealHeader:
                        ProcessDoHeal(Buffer);
                        break;
                    case endHealHeader:
                        ProcessEndHeal(Buffer);
                        break;
                    default:
                        UE_LOG(LogTemp, Warning, TEXT("Unknown packet type received: %d"), PacketType);
                        break;
                    }
                }
                else if (BytesReceived == 0 || WSAGetLastError() == WSAECONNRESET)
                {
                    UE_LOG(LogTemp, Log, TEXT("Connection closed by server."));
                    CloseConnection();
                    break;
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("recv failed with error: %ld"), WSAGetLastError());
                    CloseConnection();
                    break;
                }
            }
        });
}

void AMySocketCultistActor::ProcessCultistData(const char* Buffer)
{
    FCultistCharacterState ReceivedState;
    memcpy(&ReceivedState, Buffer + 2, sizeof(FCultistCharacterState));
    {
        FScopeLock Lock(&CultistDataMutex);
        ReceivedCultistStates.FindOrAdd(ReceivedState.PlayerID) = ReceivedState;
    }
}

void AMySocketCultistActor::ProcessTreeData(const char* Buffer)
{
    TreePacket ReceivedSkill;
    memcpy(&ReceivedSkill, Buffer, sizeof(TreePacket));

    const int Key = static_cast<int>(ReceivedSkill.casterId);
    ACharacter* FoundChar = SpawnedCharacters.FindRef(Key);
    if (!FoundChar) {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] caster %d not found"), ReceivedSkill.casterId);
        return;
    }

    ACultistCharacter* CasterCultist = Cast<ACultistCharacter>(FoundChar);
    if (!CasterCultist) {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] caster %d is not Cultist"), ReceivedSkill.casterId);
        return;
    }

    TWeakObjectPtr<ACultistCharacter> WeakCaster = CasterCultist;
    const FVector  SpawnLoc = AMySocketActor::ToUE(ReceivedSkill.SpawnLoc);
    const FRotator SpawnRot = AMySocketActor::ToUE(ReceivedSkill.SpawnRot);

    AsyncTask(ENamedThreads::GameThread, [WeakCaster, SpawnLoc, SpawnRot]() {
        ACultistCharacter* Caster = WeakCaster.Get();
        if (!IsValid(Caster)) return;

        FActorSpawnParameters Params;
        Params.Owner = Caster;
        Params.Instigator = Caster;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        if (!Caster->TreeObstacleActorClass) {
            UE_LOG(LogTemp, Error, TEXT("[Skill] %s TreeObstacleActorClass is null"), *Caster->GetName());
            return;
        }
        Caster->GetWorld()->SpawnActor<ATreeObstacleActor>(Caster->TreeObstacleActorClass, SpawnLoc, SpawnRot, Params);

        });
}

void AMySocketCultistActor::ProcessCrowSpawnData(const char* Buffer) {
    Crow ReceivedCrow;
    memcpy(&ReceivedCrow, Buffer + 2, sizeof(Crow));

    const int Key = static_cast<int>(ReceivedCrow.owner);
    ACharacter* FoundChar = SpawnedCharacters.FindRef(Key);
    if (!FoundChar) {
        UE_LOG(LogTemp, Warning, TEXT("[CrowSpawn] caster %d not found"), ReceivedCrow.owner);
        return;
    }

    ACultistCharacter* CasterCultist = Cast<ACultistCharacter>(FoundChar);
    if (!CasterCultist) {
        UE_LOG(LogTemp, Warning, TEXT("[CrowSpawn] caster %d is not Cultist"), ReceivedCrow.owner);
        return;
    }

    TWeakObjectPtr<ACultistCharacter> WeakCaster = CasterCultist;
    const FVector  SpawnLoc = AMySocketActor::ToUE(ReceivedCrow.loc);
    const FRotator SpawnRot = AMySocketActor::ToUE(ReceivedCrow.rot);

    AsyncTask(ENamedThreads::GameThread, [WeakCaster, SpawnLoc, SpawnRot]() {
        ACultistCharacter* Caster = WeakCaster.Get();
        if (!IsValid(Caster))
            return;

        FActorSpawnParameters Params;
        Params.Owner = Caster;
        Params.Instigator = Caster;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        if (!Caster->CrowClass) {
            UE_LOG(LogTemp, Error, TEXT("[CrowSpawn] %s CrowClass is null"), *Caster->GetName());
            return;
        }
        if (!Caster->CrowInstance)
        {
            Caster->CrowInstance = Caster->GetWorld()->SpawnActor<ACrowActor>(
                Caster->CrowClass, SpawnLoc, SpawnRot, Params);
            Caster->crowIsAvailable = true;
            UE_LOG(LogTemp, Warning, TEXT("[CrowSpawn] Spawned crow for caster=%s"), *Caster->GetName());
        }
    });
}

void AMySocketCultistActor::ProcessCrowData(const char* Buffer) {
    Crow ReceivedCrow;
    memcpy(&ReceivedCrow, Buffer + 2, sizeof(Crow));

    const int Key = static_cast<int>(ReceivedCrow.owner);
    ACharacter* FoundChar = SpawnedCharacters.FindRef(Key);
    if (!FoundChar) {
        UE_LOG(LogTemp, Warning, TEXT("[CrowData] caster %d not found"), ReceivedCrow.owner);
        return;
    }

    ACultistCharacter* CasterCultist = Cast<ACultistCharacter>(FoundChar);
    if (!CasterCultist) {
        UE_LOG(LogTemp, Warning, TEXT("[CrowData] caster %d is not Cultist"), ReceivedCrow.owner);
        return;
    }
    if (CasterCultist->CrowInstance && CasterCultist->crowIsAvailable) {
        // 까마귀 업데이트
        AsyncTask(ENamedThreads::GameThread, [CI = CasterCultist->CrowInstance, ReceivedCrow]() {
            if (IsValid(CI)) {
                CI->SetActorLocation(AMySocketActor::ToUE(ReceivedCrow.loc));
                CI->SetActorRotation(AMySocketActor::ToUE(ReceivedCrow.rot));
            }
        });
    }
}

void AMySocketCultistActor::ProcessCrowDisable(const char* Buffer) {
    IdOnlyPacket packet;
    memcpy(&packet, Buffer, sizeof(packet));
    
    const int Key = static_cast<int>(packet.id);
    ACharacter* FoundChar = SpawnedCharacters.FindRef(Key);
    if (!FoundChar) {
        UE_LOG(LogTemp, Warning, TEXT("[CrowDisable] caster %d not found"), packet.id);
        return;
    }

    ACultistCharacter* CasterCultist = Cast<ACultistCharacter>(FoundChar);
    if (!CasterCultist) {
        UE_LOG(LogTemp, Warning, TEXT("[CrowDisable] caster %d is not Cultist"), packet.id);
        return;
    }

    if (CasterCultist->CrowInstance)
    {
        ACrowActor* CI = CasterCultist->CrowInstance;

        AsyncTask(ENamedThreads::GameThread, [CasterCultist, CI]() {
            if (IsValid(CI))
            {
                CI->Destroy();
            }
            CasterCultist->CrowInstance = nullptr;
            CasterCultist->crowIsAvailable = false;
        });
    }
}

void AMySocketCultistActor::ProcessPoliceData(const char* Buffer)
{
    FPoliceCharacterState ReceivedState;
    memcpy(&ReceivedState, Buffer + 2, sizeof(FPoliceCharacterState));
    {
        FScopeLock Lock(&PoliceDataMutex);
        ReceivedPoliceStates.FindOrAdd(ReceivedState.PlayerID) = ReceivedState;
    }
}

void AMySocketCultistActor::ProcessDogData(const char* Buffer) {
    Dog ReceivedDog;
    memcpy(&ReceivedDog, Buffer + 2, sizeof(Dog));
    
    const int Key = static_cast<int>(ReceivedDog.owner);
    ACharacter* FoundChar = SpawnedCharacters.FindRef(Key);
    if (!FoundChar) {
        UE_LOG(LogTemp, Warning, TEXT("[DogData] caster %d not found"), ReceivedDog.owner);
        return;
    }

    APoliceCharacter* Police = Cast<APoliceCharacter>(FoundChar);
    if (!Police) {
        UE_LOG(LogTemp, Warning, TEXT("[DogData] caster %d is not Police"), ReceivedDog.owner);
        return;
    }
    if (Police->PoliceDogInstance) {
        // 개 상태 업데이트
        AsyncTask(ENamedThreads::GameThread, [PDI = Police->PoliceDogInstance, ReceivedDog]() {
            if (IsValid(PDI)) {
                PDI->SetActorLocation(AMySocketActor::ToUE(ReceivedDog.loc));
                PDI->SetActorRotation(AMySocketActor::ToUE(ReceivedDog.rot));
            }
        });
    }

}

void AMySocketCultistActor::ProcessHitData(const char* Buffer)
{
    FHitPacket ReceivedState;
    memcpy(&ReceivedState, Buffer + 2, sizeof(FHitPacket));
    {
        FScopeLock Lock(&CultistDataMutex);
        switch (ReceivedState.Weapon)
        {
        case EWeaponType::Baton:
        {
            APoliceCharacter* Attacker = Cast<APoliceCharacter>(SpawnedCharacters.FindRef(ReceivedState.AttackerID));
            if (not Attacker) {
                UE_LOG(LogTemp, Error, TEXT("ProcessHitData failed with error spawn attacker: %d"), ReceivedState.AttackerID);
                return;
            }
            if (ReceivedState.TargetID == my_ID)
            {
                if (not MyCharacter)
                {
                    UE_LOG(LogTemp, Error, TEXT("MyCharacter is null for self-hit!"));
                    return;
                }
                AsyncTask(ENamedThreads::GameThread, [this, Attacker]()
                    {
                        MyCharacter->OnHitbyBaton(Attacker->GetActorLocation(), BatonAttackDamage);
                    });
            }
            else {
                ACultistCharacter* Target = Cast<ACultistCharacter>(SpawnedCharacters.FindRef(ReceivedState.TargetID));
                if (not Target) {
                    UE_LOG(LogTemp, Error, TEXT("ProcessHitData failed with error spawn Target: %d"), ReceivedState.TargetID);
                    return;
                }
                AsyncTask(ENamedThreads::GameThread, [this, Target, Attacker]()
                    {
                        Target->OnHitbyBaton(Attacker->GetActorLocation(), BatonAttackDamage);
                    });
            }

            break;
        }
        case EWeaponType::Pistol: 
        {
            if (ReceivedState.TargetID == my_ID)
            {
                if (not MyCharacter)
                {
                    UE_LOG(LogTemp, Error, TEXT("MyCharacter is null for self-hit!"));
                    return;
                }
                AsyncTask(ENamedThreads::GameThread, [this]()
                    {
                        MyCharacter->TakeDamage(PistolAttackDamage);
                    });
            }
            else {
                ACultistCharacter* Target = Cast<ACultistCharacter>(SpawnedCharacters.FindRef(ReceivedState.TargetID));
                if (not Target) {
                    UE_LOG(LogTemp, Error, TEXT("ProcessHitData failed with error spawn Target: %d"), ReceivedState.TargetID);
                    return;
                }
                AsyncTask(ENamedThreads::GameThread, [this, Target]()
                    {
                        Target->TakeDamage(PistolAttackDamage);
                    });
            }
            UE_LOG(LogTemp, Error, TEXT("EWeaponType received: %d"), ReceivedState.Weapon);
            break;
        }
        case EWeaponType::Taser:
        {
            APoliceCharacter* Attacker = Cast<APoliceCharacter>(SpawnedCharacters.FindRef(ReceivedState.AttackerID));
            if (not Attacker) {
                UE_LOG(LogTemp, Error, TEXT("ProcessHitData failed with error spawn attacker: %d"), ReceivedState.AttackerID);
                return;
            }
            if (ReceivedState.TargetID == my_ID)
            {
                if (not MyCharacter)
                {
                    UE_LOG(LogTemp, Error, TEXT("MyCharacter is null for self-hit!"));
                    return;
                }
                AsyncTask(ENamedThreads::GameThread, [this, Attacker]()
                    {
                        MyCharacter->GotHitTaser(Attacker);
                    });
            }
            else {
                ACultistCharacter* Target = Cast<ACultistCharacter>(SpawnedCharacters.FindRef(ReceivedState.TargetID));
                if (not Target) {
                    UE_LOG(LogTemp, Error, TEXT("ProcessHitData failed with error spawn Target: %d"), ReceivedState.TargetID);
                    return;
                }
                AsyncTask(ENamedThreads::GameThread, [this, Target, Attacker]()
                    {
                        Target->GotHitTaser(Attacker);
                    });
            }
            break;
        }
        default:
            UE_LOG(LogTemp, Error, TEXT("EWeaponType Error: %d"), ReceivedState.Weapon);
            break;
        }
    }
}

void AMySocketCultistActor::ProcessConnection(const char* Buffer) {
    unsigned char connectedId = static_cast<unsigned char>(Buffer[2]);
    unsigned char role = static_cast<unsigned char>(Buffer[3]);
    
    if (my_ID == -1) {
        my_ID = static_cast<int>(connectedId);
        UE_LOG(LogTemp, Warning, TEXT("Connected. My ID is: %d"), my_ID);

        if (MyCharacter) {
            MyCharacter->my_ID = my_ID;
        }
    }
    else {
        AsyncTask(ENamedThreads::GameThread, [this, connectedId, role]() mutable
            {
                if (role == 0) // Cultist
                {
                    this->SpawnCultistCharacter(connectedId);
                }
                else if (role == 1) // Police
                {
                    this->SpawnPoliceCharacter(connectedId);
                }
                else if (role == 99) { //PoliceAi
                    this->SpawnPoliceAICharacter(connectedId);
                }
            });
    }
}

void AMySocketCultistActor::ProcessDisconnection(const char* Buffer)
{
    int DisconnectedID = static_cast<int>(static_cast<unsigned char>(Buffer[2]));
    if (DisconnectedID == my_ID)
    {
        CloseConnection();
    }
    else {
        SafeDestroyCharacter(DisconnectedID);
    }
}

void AMySocketCultistActor::SendDisable() 
{
    if (ClientSocket != INVALID_SOCKET)
    {
        IdOnlyPacket Packet;
        Packet.header = disableHeader;
        Packet.size = sizeof(IdOnlyPacket);
        Packet.id = my_ID;
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(IdOnlyPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendDisable failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CloseConnection();
    }
}

void AMySocketCultistActor::SendTree(FVector SpawnLoc, FRotator SpawnRot)
{
    if (ClientSocket != INVALID_SOCKET)
    {
        TreePacket Packet;
        Packet.header = treeHeader;
        Packet.size = sizeof(TreePacket);
        Packet.casterId = MyCharacter->my_ID;
        Packet.SpawnLoc = AMySocketActor::ToNet(SpawnLoc);
        Packet.SpawnRot = AMySocketActor::ToNet(SpawnRot);
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(TreePacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendSkill failed with error: %ld"), WSAGetLastError());
        }
        AsyncTask(ENamedThreads::GameThread, [this, SpawnLoc, SpawnRot]() {
                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = this;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                GetWorld()->SpawnActor<AProceduralBranchActor>(MyCharacter->ProceduralBranchActorClass, SpawnLoc, SpawnRot, SpawnParams);
        });       
    }
    else
    {
        CloseConnection();
    }
}

void AMySocketCultistActor::SendCrowSpawn(FVector SpawnLoc, FRotator SpawnRot)
{
    if (ClientSocket != INVALID_SOCKET)
    {
        CrowPacket Packet;
        Packet.header = crowSpawnHeader;
        Packet.size = sizeof(CrowPacket);
        Packet.crow.owner = MyCharacter->my_ID;
        Packet.crow.loc = AMySocketActor::ToNet(SpawnLoc);
        Packet.crow.rot = AMySocketActor::ToNet(SpawnRot);
        Packet.crow.is_alive = MyCharacter->crowIsAvailable;
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(CrowPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendCrowSpawn failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CloseConnection();
    }
}

void AMySocketCultistActor::SendCrowData() {
    if (ClientSocket != INVALID_SOCKET)
    {
        CrowPacket Packet;
        Packet.header = crowDataHeader;
        Packet.size = sizeof(CrowPacket);
        Packet.crow = GetCrow();
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(CrowPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendCrowData failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CloseConnection();
    } 
}

void AMySocketCultistActor::SendCrowDisable() {
    if (ClientSocket != INVALID_SOCKET)
    {
        IdOnlyPacket Packet;
        Packet.header = crowDisableHeader;
        Packet.size = sizeof(IdOnlyPacket);
        Packet.id = MyCharacter->my_ID;
        UE_LOG(LogTemp, Warning, TEXT("my_ID = %d"), MyCharacter->my_ID);
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(IdOnlyPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendCrowDisable failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CloseConnection();
    }
}

void AMySocketCultistActor::SendPlayerData()
{
    if (ClientSocket != INVALID_SOCKET)
    {
        CultistPacket Packet;
        Packet.header = cultistHeader;
        Packet.size = sizeof(CultistPacket);
        Packet.state = GetCharacterState();

        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(CultistPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendPlayerData failed with error: %ld"), WSAGetLastError());
        }
        if (MyCharacter->crowIsAvailable) {
            SendCrowData();
        }
    }
    else
    {
        CloseConnection();
    }
}
    
FCultistCharacterState AMySocketCultistActor::GetCharacterState()
{
    FCultistCharacterState State;
    State.PlayerID = my_ID;

    // 위치 및 회전 설정
    State.PositionX = MyCharacter->GetActorLocation().X;
    State.PositionY = MyCharacter->GetActorLocation().Y;
    State.PositionZ = MyCharacter->GetActorLocation().Z;
    State.RotationPitch = MyCharacter->GetActorRotation().Pitch;
    State.RotationYaw = MyCharacter->GetActorRotation().Yaw;
    State.RotationRoll = MyCharacter->GetActorRotation().Roll;

    // 속도 및 Speed 계산
    FVector Velocity = MyCharacter->GetVelocity();
    State.VelocityX = Velocity.X;
    State.VelocityY = Velocity.Y;
    State.VelocityZ = Velocity.Z;
    State.Speed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

    // Crouch 상태
    State.Crouch = MyCharacter->bIsCrouched;

    ACultistCharacter* CultistChar = Cast<ACultistCharacter>(MyCharacter);
    if (CultistChar)
    {
        State.ABP_IsPerforming = CultistChar->bIsPerformingRitual;
        State.ABP_IsHitByAnAttack = CultistChar->bIsHitByAnAttack;
        State.ABP_IsFrontKO = CultistChar->bIsFrontFallen;
        State.ABP_IsElectric = CultistChar->bIsElectric;
        State.ABP_TTStun = CultistChar->TurnToStun;
        State.ABP_TTGetUp = CultistChar->TurnToGetUp;
        State.ABP_IsDead = CultistChar->bIsDead;
        State.ABP_IsStunned = CultistChar->bIsStunned;
        State.ABP_DoHeal = CultistChar->ABP_DoHeal;
        State.ABP_GetHeal = CultistChar->ABP_GetHeal;
        State.bIsPakour = CultistChar->bIsPakour;

        //UE_LOG(LogTemp, Error, TEXT("Client ABP_TTStun: %d, ABP_IsDead: %d"), State.ABP_TTStun, CultistChar->bIsDead);
        State.CurrentHealth = CultistChar->CurrentHealth;
    }
    return State;
}

Crow AMySocketCultistActor::GetCrow()
{
    Crow crow;
    crow.owner = MyCharacter->my_ID;
    if (MyCharacter->CrowInstance)
    {
        crow.loc = AMySocketActor::ToNet(MyCharacter->CrowInstance->GetActorLocation());
        crow.rot = AMySocketActor::ToNet(MyCharacter->CrowInstance->GetActorRotation());
        crow.is_alive = MyCharacter->crowIsAvailable;
        // 까마귀 상태 추가
    }

    return crow;
}

void AMySocketCultistActor::ProcessCharacterUpdates()
{
    {
        FScopeLock Lock(&CultistDataMutex);
        for (auto& Pair : ReceivedCultistStates)
        {
            const FCultistCharacterState& State = Pair.Value;

            if (ACharacter* FoundChar = SpawnedCharacters.FindRef(Pair.Value.PlayerID))
            {
                if (ACultistCharacter* CultistChar = Cast<ACultistCharacter>(FoundChar)) 
                {
                    UpdateCultistState(CultistChar, State);
                }
            }
            else {
                UE_LOG(LogTemp, Warning, TEXT("No Cultist PlayerID %d"), Pair.Value.PlayerID);
                KeysToRemove.Add(Pair.Value.PlayerID);
            }
        }
    }   
    for (int32 Key : KeysToRemove)
    {
        ReceivedCultistStates.Remove(Key);
    }
    KeysToRemove.Reset();
    {
        FScopeLock Lock(&PoliceDataMutex);
        for (auto& Pair : ReceivedPoliceStates)
        {
            const FPoliceCharacterState& PoliceState = Pair.Value;

            if (ACharacter* FoundChar = SpawnedCharacters.FindRef(Pair.Value.PlayerID))
            {
                if (APoliceCharacter* PoliceChar = Cast<APoliceCharacter>(FoundChar))
                {
                    UpdatePoliceState(PoliceChar, PoliceState);
                }
                else {
                    UE_LOG(LogTemp, Warning, TEXT("No Police PlayerID %d"), Pair.Value.PlayerID);
                }
            }
        }
    }
}

void AMySocketCultistActor::UpdateCultistState(ACharacter* Character, const FCultistCharacterState& State)
{
    float InterpSpeed = 30.0f; // 보간 속도
    const float DeltaTime = GetWorld()->GetDeltaSeconds();

    FVector CurrentLocation = Character->GetActorLocation();
    FVector TargetLocation(State.PositionX, State.PositionY, State.PositionZ);

    if (State.Crouch)
    {
        TargetLocation.Z += 50.0f;
    }

    FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, InterpSpeed);
    Character->SetActorLocation(NewLocation);
    Character->SetActorRotation(FRotator(State.RotationPitch, State.RotationYaw, State.RotationRoll));

    // 애니메이션 상태 업데이트
    if (USkeletalMeshComponent* Mesh = Character->GetMesh())
    {
        UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
        if (AnimInstance)
        {
            UpdateCultistAnimInstanceProperties(AnimInstance, State);
        }
    }
}

void AMySocketCultistActor::UpdateCultistAnimInstanceProperties(UAnimInstance* AnimInstance, const FCultistCharacterState& State)
{
    // Velocity 업데이트
    FProperty* VelocityProperty = AnimInstance->GetClass()->FindPropertyByName(FName("Velocity"));
    if (VelocityProperty && VelocityProperty->IsA<FStructProperty>())
    {
        FVector Velocity(State.VelocityX, State.VelocityY, State.VelocityZ);
        FStructProperty* StructProp = CastFieldChecked<FStructProperty>(VelocityProperty);
        void* StructContainer = StructProp->ContainerPtrToValuePtr<void>(AnimInstance);
        if (StructContainer)
        {
            FMemory::Memcpy(StructContainer, &Velocity, sizeof(FVector));
        }
    }

    // Speed 업데이트
    FProperty* SpeedProperty = AnimInstance->GetClass()->FindPropertyByName(FName("Speed"));
    if (SpeedProperty && SpeedProperty->IsA<FDoubleProperty>())
    {
        FDoubleProperty* DoubleProp = CastFieldChecked<FDoubleProperty>(SpeedProperty);
        DoubleProp->SetPropertyValue_InContainer(AnimInstance, State.Speed);
    }
    
    FProperty* IsCrouchProperty = AnimInstance->GetClass()->FindPropertyByName(FName("Crouch"));
    if (IsCrouchProperty && IsCrouchProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsCrouchProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.Crouch));
    }

    FProperty* IsABP_IsPerformingProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsPerforming"));
    if (IsABP_IsPerformingProperty && IsABP_IsPerformingProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_IsPerformingProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.ABP_IsPerforming));
    }

    FProperty* IsABP_IsHitByAnAttackProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsHitByAnAttack"));
    if (IsABP_IsHitByAnAttackProperty && IsABP_IsHitByAnAttackProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_IsHitByAnAttackProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.ABP_IsHitByAnAttack));
    }

    FProperty* IsABP_IsFrontKOProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsFrontKO"));
    if (IsABP_IsFrontKOProperty && IsABP_IsFrontKOProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_IsFrontKOProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.ABP_IsFrontKO));
    }

    FProperty* IsABP_IsElectricProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsElectric"));
    if (IsABP_IsElectricProperty && IsABP_IsElectricProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_IsElectricProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.ABP_IsElectric));
    }

    FProperty* IsABP_TTStunProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_TTStun"));
    if (IsABP_TTStunProperty && IsABP_TTStunProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_TTStunProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.ABP_TTStun));
    }

    FProperty* IsABP_TTGetUpProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_TTGetUp"));
    if (IsABP_TTGetUpProperty && IsABP_TTGetUpProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_TTGetUpProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.ABP_TTGetUp));
    }

    FProperty* IsABP_IsDeadProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsDead"));
    if (IsABP_IsDeadProperty && IsABP_IsDeadProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_IsDeadProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.ABP_IsDead));
    }

    FProperty* IsABP_IsStunnedProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsStunned"));
    if (IsABP_IsStunnedProperty && IsABP_IsStunnedProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_IsStunnedProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.ABP_IsStunned));
    }

    FProperty* IsABP_IsPakourProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsPakour"));
    if (IsABP_IsPakourProperty && IsABP_IsPakourProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_IsPakourProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.bIsPakour));
    }

    FProperty* IsABP_DoHealProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_DoHeal"));
    if (IsABP_DoHealProperty && IsABP_DoHealProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_DoHealProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.ABP_DoHeal));
    }

    FProperty* IsABP_GetHealProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_GetHeal"));
    if (IsABP_GetHealProperty && IsABP_GetHealProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_GetHealProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.ABP_GetHeal));
    }
}

void AMySocketCultistActor::SpawnCultistCharacter(const unsigned char PlayerID)
{
    // 이미 캐릭터가 존재하면 아무 작업도 하지 않음
    if (SpawnedCharacters.Contains(PlayerID))
    {
        UE_LOG(LogTemp, Warning, TEXT("Character already exists: %d"), PlayerID);
        return;
    }
    if (PlayerID == my_ID) {
        UE_LOG(LogTemp, Warning, TEXT("SpawnPoliceCharacter Failed. %d is my_ID"), PlayerID);
        return;
    }
    if (!GI || !GI->CultistClientClass)
    {
        UE_LOG(LogTemp, Error, TEXT("GI or CultistClientClass is null"));
        return;
    }
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ACultistCharacter* NewCharacter = GetWorld()->SpawnActor<ACultistCharacter>(
        GI->CultistClientClass,
        FVector(CultistDummyState.PositionX, CultistDummyState.PositionY, CultistDummyState.PositionZ),
        FRotator(CultistDummyState.RotationPitch, CultistDummyState.RotationYaw, CultistDummyState.RotationRoll),
        SpawnParams
    );

    if (NewCharacter)
    {
        SpawnedCharacters.Add(PlayerID, NewCharacter);
        ReceivedCultistStates.Add(PlayerID, CultistDummyState);
        UE_LOG(LogTemp, Log, TEXT("Spawned new cultist character for PlayerID=%d"), PlayerID);

        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC && PC->IsLocalController())
        {
            APawn* MyPawn = PC->GetPawn();
            if (MyPawn)
            {
                // ChildActorComponent 찾아서
                UChildActorComponent* CAC = MyPawn->FindComponentByClass<UChildActorComponent>();
                if (CAC)
                {
                    ACameraActor* CamActor = Cast<ACameraActor>(CAC->GetChildActor());
                    if (CamActor)
                    {
                        PC->SetViewTargetWithBlend(CamActor, 0.f);
                    }
                }
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to spawn character for PlayerID=%d"), PlayerID);
    }
    
}

void AMySocketCultistActor::UpdatePoliceState(ACharacter* Character, const FPoliceCharacterState& State)
{
    float InterpSpeed = 30.0f; // 보간 속도
    const float DeltaTime = GetWorld()->GetDeltaSeconds();

    FVector CurrentLocation = Character->GetActorLocation();
    FVector TargetLocation(State.PositionX, State.PositionY, State.PositionZ);

    if (State.bIsCrouching)
    {
        TargetLocation.Z += 50.0f;
    }

    FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, InterpSpeed);
    Character->SetActorLocation(NewLocation);
    Character->SetActorRotation(FRotator(State.RotationPitch, State.RotationYaw, State.RotationRoll));

    // 무기 상태 업데이트
    APoliceCharacter* PoliceChar = Cast<APoliceCharacter>(Character);
    if (PoliceChar)
    {
        PoliceChar->CurrentWeapon = State.CurrentWeapon; // 네트워크에서 받은 무기 타입으로 업데이트
        PoliceChar->UpdateWeaponVisibility();
    }

    // 애니메이션 상태 업데이트
    if (USkeletalMeshComponent* Mesh = Character->GetMesh())
    {
        UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
        if (AnimInstance)
        {
            UpdatePoliceAnimInstanceProperties(AnimInstance, State);
        }
    }
}

void AMySocketCultistActor::UpdatePoliceAnimInstanceProperties(UAnimInstance* AnimInstance, const FPoliceCharacterState& State)
{
    // Velocity 업데이트
    FProperty* VelocityProperty = AnimInstance->GetClass()->FindPropertyByName(FName("Velocity"));
    if (VelocityProperty && VelocityProperty->IsA<FStructProperty>())
    {
        FVector Velocity(State.VelocityX, State.VelocityY, State.VelocityZ);
        FStructProperty* StructProp = CastFieldChecked<FStructProperty>(VelocityProperty);
        void* StructContainer = StructProp->ContainerPtrToValuePtr<void>(AnimInstance);
        if (StructContainer)
        {
            FMemory::Memcpy(StructContainer, &Velocity, sizeof(FVector));
        }
    }

    // Speed 업데이트
    {
        FProperty* SpeedProperty = AnimInstance->GetClass()->FindPropertyByName(FName("Speed"));
        if (SpeedProperty && SpeedProperty->IsA<FDoubleProperty>())
        {
            FDoubleProperty* DoubleProp = CastFieldChecked<FDoubleProperty>(SpeedProperty);
            DoubleProp->SetPropertyValue_InContainer(AnimInstance, State.Speed);
        }
    }

    // IsCrouching 업데이트
    FProperty* IsCrouchingProperty = AnimInstance->GetClass()->FindPropertyByName(FName("Crouch"));
    if (IsCrouchingProperty && IsCrouchingProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsCrouchingProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, State.bIsCrouching);
    }

    // ABP_IsAiming 업데이트
    FProperty* ABP_IsAimingProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsAiming"));
    if (ABP_IsAimingProperty && ABP_IsAimingProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(ABP_IsAimingProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, State.bIsAiming);
    }

    // IsAttacking 업데이트
    FProperty* IsAttackingProperty = AnimInstance->GetClass()->FindPropertyByName(FName("IsAttacking"));
    if (IsAttackingProperty && IsAttackingProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsAttackingProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, State.bIsAttacking);
    }

    // WeaponType 확인
    if (State.CurrentWeapon == EWeaponType::Baton)
    {
        // IsBaton = true, 나머지 = false
        {
            FProperty* IsBatonProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsBaton"));
            if (IsBatonProp && IsBatonProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsBatonProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, true);
            }
        }
        {
            FProperty* IsPistolProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsPistol"));
            if (IsPistolProp && IsPistolProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsPistolProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, false);
            }
        }
        {
            FProperty* IsTaserProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsTaser"));
            if (IsTaserProp && IsTaserProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsTaserProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, false);
            }
        }
    }
    else if (State.CurrentWeapon == EWeaponType::Pistol)
    {
        // IsPistol = true, 나머지 = false
        {
            FProperty* IsBatonProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsBaton"));
            if (IsBatonProp && IsBatonProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsBatonProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, false);
            }
        }
        {
            FProperty* IsPistolProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsPistol"));
            if (IsPistolProp && IsPistolProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsPistolProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, true);
            }
        }
        {
            FProperty* IsTaserProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsTaser"));
            if (IsTaserProp && IsTaserProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsTaserProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, false);
            }
        }
    }
    else if (State.CurrentWeapon == EWeaponType::Taser)
    {
        // IsTaser = true, 나머지 = false
        {
            FProperty* IsBatonProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsBaton"));
            if (IsBatonProp && IsBatonProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsBatonProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, false);
            }
        }
        {
            FProperty* IsPistolProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsPistol"));
            if (IsPistolProp && IsPistolProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsPistolProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, false);
            }
        }
        {
            FProperty* IsTaserProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsTaser"));
            if (IsTaserProp && IsTaserProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsTaserProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, true);
            }
        }
    }

    // ABP_IsPakour 업데이트
    FProperty* ABP_IsPakourProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsPakour"));
    if (ABP_IsPakourProperty && ABP_IsPakourProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(ABP_IsPakourProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, State.bIsPakour);
    }

    // EVaultingType 업데이트
    if (State.CurrentVaultType == EVaultingType::OneHandVault)
    {
        // IsOneHand = true, 나머지 = false
        {
            FProperty* IsOneHandProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsOneHand"));
            if (IsOneHandProp && IsOneHandProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsOneHandProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, true);
            }
        }
        {
            FProperty* IsTwoHandProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsTwoHand"));
            if (IsTwoHandProp && IsTwoHandProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsTwoHandProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, false);
            }
        }
        {
            FProperty* IsFlipProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsFlip"));
            if (IsFlipProp && IsFlipProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsFlipProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, false);
            }
        }
    }
    else if (State.CurrentVaultType == EVaultingType::TwoHandVault)
    {
        // IsTwoHand = true, 나머지 = false
        {
            FProperty* IsOneHandProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsOneHand"));
            if (IsOneHandProp && IsOneHandProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsOneHandProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, false);
            }
        }
        {
            FProperty* IsTwoHandProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsTwoHand"));
            if (IsTwoHandProp && IsTwoHandProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsTwoHandProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, true);
            }
        }
        {
            FProperty* IsFlipProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsFlip"));
            if (IsFlipProp && IsFlipProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsFlipProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, false);
            }
        }
    }
    else if (State.CurrentVaultType == EVaultingType::FrontFlip)
    {
        // IsFlip = true, 나머지 = false
        {
            FProperty* IsOneHandProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsOneHand"));
            if (IsOneHandProp && IsOneHandProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsOneHandProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, false);
            }
        }
        {
            FProperty* IsTwoHandProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsTwoHand"));
            if (IsTwoHandProp && IsTwoHandProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsTwoHandProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, false);
            }
        }
        {
            FProperty* IsFlipProp = AnimInstance->GetClass()->FindPropertyByName(FName("IsFlip"));
            if (IsFlipProp && IsFlipProp->IsA<FBoolProperty>())
            {
                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsFlipProp);
                BoolProp->SetPropertyValue_InContainer(AnimInstance, true);
            }
        }
    }

    // bIsShooting 업데이트
    FProperty* IsShootingProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsShooting"));
    if (IsShootingProperty && IsShootingProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsShootingProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, State.bIsShooting);
    }
}

void AMySocketCultistActor::SpawnPoliceCharacter(const unsigned char PlayerID)
{
    // 이미 캐릭터가 존재하면 아무 작업도 하지 않음
    if (SpawnedCharacters.Contains(PlayerID))
    {
        UE_LOG(LogTemp, Warning, TEXT("Character already exists: %d"), PlayerID);
        return;
    }
    if (PlayerID == my_ID) {
        UE_LOG(LogTemp, Warning, TEXT("SpawnPoliceCharacter Failed. %d is my_ID"), PlayerID);
        return;
    }
    if (!GI || !GI->PoliceClientClass)
    {
        UE_LOG(LogTemp, Error, TEXT("GI or PoliceClientClass is null"));
        return;
    }
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    APoliceCharacter* NewCharacter = GetWorld()->SpawnActor<APoliceCharacter>(
        GI->PoliceClientClass,
        FVector(PoliceDummyState.PositionX, PoliceDummyState.PositionY, PoliceDummyState.PositionZ),
        FRotator(PoliceDummyState.RotationPitch, PoliceDummyState.RotationYaw, PoliceDummyState.RotationRoll),
        SpawnParams
    );
    if (NewCharacter)
    {
        SpawnedCharacters.Add(PlayerID, NewCharacter);
        ReceivedPoliceStates.Add(PlayerID, PoliceDummyState);
        UE_LOG(LogTemp, Log, TEXT("Spawned new police character for PlayerID=%d"), PlayerID);

        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC && PC->IsLocalController())
        {
            APawn* MyPawn = PC->GetPawn();
            if (MyPawn)
            {
                // ChildActorComponent 찾아서
                UChildActorComponent* CAC = MyPawn->FindComponentByClass<UChildActorComponent>();
                if (CAC)
                {
                    ACameraActor* CamActor = Cast<ACameraActor>(CAC->GetChildActor());
                    if (CamActor)
                    {
                        PC->SetViewTargetWithBlend(CamActor, 0.f);
                    }
                }
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to spawn character for PlayerID=%d"), PlayerID);
    }
    
}

void AMySocketCultistActor::SpawnPoliceAICharacter(const unsigned char PlayerID)
{
    // 이미 캐릭터가 존재하면 아무 작업도 하지 않음
    if (SpawnedCharacters.Contains(PlayerID))
    {
        UE_LOG(LogTemp, Warning, TEXT("Character already exists: %d"), PlayerID);
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    UClass* BP_ClientCharacter = LoadClass<ACharacter>(nullptr, TEXT("/Game/Cult_Custom/Characters/Police/BP_PoliceCharacter_Client.BP_PoliceCharacter_Client_C"));

    if (BP_ClientCharacter)
    {
        APoliceCharacter* NewCharacter = GetWorld()->SpawnActor<APoliceCharacter>(
            BP_ClientCharacter,
            FVector(PoliceDummyState.PositionX, PoliceDummyState.PositionY, PoliceDummyState.PositionZ),
            FRotator(PoliceDummyState.RotationPitch, PoliceDummyState.RotationYaw, PoliceDummyState.RotationRoll),
            SpawnParams
        );
        if (NewCharacter)
        {
            SpawnedCharacters.Add(PlayerID, NewCharacter);
            ReceivedPoliceStates.Add(PlayerID, PoliceDummyState);
            UE_LOG(LogTemp, Log, TEXT("Spawned new character for PlayerID=%d"), PlayerID);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn character for PlayerID=%d"), PlayerID);
        }
    }
    // ready Header send
    uint8 PacketData = readyHeader;
    int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&PacketData), sizeof(PacketData), 0);
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendReadyPacket failed with error: %ld"), WSAGetLastError());
    }
}

void AMySocketCultistActor::ProcessParticleData(const char* Buffer) {
    FImpactPacket ReceivedImpact;
    memcpy(&ReceivedImpact, Buffer + 2, sizeof(FImpactPacket));
    // Particles.Add(ReceivedImpact);
    AsyncTask(ENamedThreads::GameThread, [this, ReceivedImpact]() {
        SpawnImpactEffect(ReceivedImpact);
        });
}

bool IsVectorFinite(const FVector& Vec) {
    return FMath::IsFinite(Vec.X) && FMath::IsFinite(Vec.Y) && FMath::IsFinite(Vec.Z);
}

bool IsRotatorFinite(const FRotator& Rot) {
    return FMath::IsFinite(Rot.Pitch) && FMath::IsFinite(Rot.Yaw) && FMath::IsFinite(Rot.Roll);
}

void AMySocketCultistActor::SpawnImpactEffect(const FImpactPacket& ReceivedImpact)
{
    FVector ImpactPoint(ReceivedImpact.ImpactX, ReceivedImpact.ImpactY, ReceivedImpact.ImpactZ);
    FVector ImpactNormal(ReceivedImpact.NormalX, ReceivedImpact.NormalY, ReceivedImpact.NormalZ);
    FRotator ImpactRotation = ImpactNormal.Rotation();
    FVector MuzzleLoc(ReceivedImpact.MuzzleX, ReceivedImpact.MuzzleY, ReceivedImpact.MuzzleZ);
    FRotator MuzzleRot(ReceivedImpact.MuzzlePitch, ReceivedImpact.MuzzleYaw, ReceivedImpact.MuzzleRoll);
    if (!IsVectorFinite(ImpactPoint) || !IsVectorFinite(ImpactNormal) ||
        !IsVectorFinite(MuzzleLoc) || !IsRotatorFinite(MuzzleRot) || !IsRotatorFinite(ImpactRotation))
    {
        UE_LOG(LogTemp, Error, TEXT("ImpactPacket contains NaN or Infinite values!"));
        return;
    }

    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        GetWorld(), GI->NGParticleAsset,
        ImpactPoint,
        ImpactRotation,
        FVector(1.0f), true, true,
        ENCPoolMethod::None, true
    );

    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        GetWorld(), GI->MuzzleEffect,
        MuzzleLoc,
        MuzzleRot
    );
}

void AMySocketCultistActor::HideCharacter(int PlayerID, bool bHide) {
    AsyncTask(ENamedThreads::GameThread, [this, PlayerID, bHide]()
        {
            if (ACharacter* Char = SpawnedCharacters.FindRef(PlayerID)) {
                Char->SetActorHiddenInGame(bHide);
                Char->SetActorEnableCollision(!bHide);
                Char->SetActorTickEnabled(!bHide);

                UE_LOG(LogTemp, Log, TEXT("Character %d %s"),
                    PlayerID, bHide ? TEXT("hidden") : TEXT("unhidden"));
            }
        });
}

void AMySocketCultistActor::SendTryHeal()
{
    if (ClientSocket != INVALID_SOCKET)
    {
        IdOnlyPacket Packet;
        Packet.header = tryHealHeader;
        Packet.size = sizeof(IdOnlyPacket);
        Packet.id = MyCharacter->my_ID;
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(IdOnlyPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("sendTryHeal failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CloseConnection();
    }
}

void AMySocketCultistActor::ProcessDoHeal(const char* Buffer) {
    MovePacket Received;
    memcpy(&Received, Buffer, sizeof(MovePacket));

    const FVector Goal = AMySocketActor::ToUE(Received.SpawnLoc);
    const FRotator Face = AMySocketActor::ToUE(Received.SpawnRot);
    const bool isHealer = Received.isHealer;

    AsyncTask(ENamedThreads::GameThread, [this, Goal, Face, isHealer]() {
        if (!MyCharacter) return;

        // set Actor Rotation
        MyCharacter->SetActorRotation(Face);

        // Ai Move to
        AController* C = MyCharacter->GetController();
        /*if (AAIController* AICon = Cast<AAIController>(C))
        {
            UE_LOG(LogTemp, Error, TEXT("Cast AAIController"));
            FAIMoveRequest MoveReq;
            MoveReq.SetGoalLocation(Goal);
            MoveReq.SetAcceptanceRadius(100.f);
            MoveReq.SetUsePathfinding(true);
            FNavPathSharedPtr Path;
            AICon->MoveTo(MoveReq, &Path);
        }
        else */if (APlayerController* PC = Cast<APlayerController>(C))
        {
            UE_LOG(LogTemp, Error, TEXT("Cast APlayerController"));
            UAIBlueprintHelperLibrary::SimpleMoveToLocation(PC, Goal);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ProcessDoHeal Fail to Cast APlayerController"));
            return;
            //MyCharacter->SetActorLocation(Goal);
        }
        // isHealer 여부에 따른 anim 재생
        if (USkeletalMeshComponent* Mesh = MyCharacter->GetMesh())
        {
            if (UAnimInstance* Anim = Mesh->GetAnimInstance())
            {
                if (isHealer) {
                    MyCharacter->ABP_DoHeal = true;
                    if (MyCharacter->AS_BandageFriendSquat1_Montage)
                    {
                        Anim->Montage_Play(MyCharacter->AS_BandageFriendSquat1_Montage, 1.0f);
                        Anim->OnMontageEnded.AddDynamic(
                            this, &AMySocketCultistActor::HandleMontageEnded);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("ProcessDoHeal: AS_BandageFriendSquat1_Montage is null"));
                    }
                }
                else {
                    MyCharacter->ABP_GetHeal = true;
                    if (MyCharacter->AS_BandageArmSitting1_Montage)
                    {
                        Anim->Montage_Play(MyCharacter->AS_BandageArmSitting1_Montage, 1.0f);

                        // NotifyBegin에서 전환
                        BoundAnimInstance = Anim;
                        Anim->OnPlayMontageNotifyBegin.AddDynamic(this, &AMySocketCultistActor::HandleMontageSitNotifyBegin);
                        Anim->OnMontageEnded.AddDynamic(this, &AMySocketCultistActor::HandleMontageEnded);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("ProcessDoHeal: AS_BandageArmSitting1_Montage is null"));
                    }
                }
            }
            else {
                UE_LOG(LogTemp, Error, TEXT("ProcessDoHeal Fail to get UAnimInstance"));
                return;
            }
        }
        else {
            UE_LOG(LogTemp, Error, TEXT("ProcessDoHeal Fail to get USkeletalMeshComponent"));
            return;
        }
        });
        
}

void AMySocketCultistActor::SendEndHeal()
{
    if (ClientSocket != INVALID_SOCKET)
    {
        NoticePacket Packet;
        Packet.header = endHealHeader;
        Packet.size = sizeof(NoticePacket);
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(NoticePacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendEndHeal failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CloseConnection();
    }
}

void AMySocketCultistActor::ProcessEndHeal(const char* Buffer) {
    BoolPacket Received;
    //memcpy(&Received, Buffer, sizeof(MovePacket));
    memcpy(&Received, Buffer, sizeof(BoolPacket));
    if (Received.result) {
        // 치료 성공 - hp회복, 상태 복구
        MyCharacter->CurrentHealth = 100.0f;
    }
    else {
        // 치료 실패 ( 움직임 )
        

    }
    if (USkeletalMeshComponent* Mesh = MyCharacter->GetMesh())
    {
        if (UAnimInstance* Anim = Mesh->GetAnimInstance())
        {
            if (MyCharacter->AS_WoundedSitting1_Montage)
            {
                Anim->Montage_Stop(0.2f, MyCharacter->AS_WoundedSitting1_Montage);
            }
            if (MyCharacter->AS_BandageFriendSquat1_Montage)
            {
                Anim->Montage_Stop(0.2f, MyCharacter->AS_BandageFriendSquat1_Montage);
            }
        }
    }
    MyCharacter->ABP_DoHeal = false;
    MyCharacter->ABP_GetHeal = false;
}

void AMySocketCultistActor::SendStartRitual(uint8_t ritual_id) {
    // 제단 시작
    if (ClientSocket != INVALID_SOCKET)
    {
        RitualNoticePacket Packet;
        Packet.header = ritualStartHeader;
        Packet.size = sizeof(RitualNoticePacket);
        Packet.id = MyCharacter->my_ID;
        Packet.ritual_id = ritual_id;
        Packet.reason = 0;
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(RitualNoticePacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendStartRitual failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CloseConnection();
    }
}

void AMySocketCultistActor::SendRitualSkillCheck(uint8_t ritual_id, uint8_t reason) {
    // reason 1 -> skill check suc
    // reason 2 -> skill check fail
    if (ClientSocket != INVALID_SOCKET)
    {
        RitualNoticePacket Packet;
        Packet.header = ritualDataHeader;
        Packet.size = sizeof(RitualNoticePacket);
        Packet.id = MyCharacter->my_ID;
        Packet.ritual_id = ritual_id;
        Packet.reason = reason;
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(RitualNoticePacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendRitualSkillCheck failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CloseConnection();
    }
}

void AMySocketCultistActor::SendEndRitual(uint8_t ritual_id, uint8_t reason) {
    // 제단 손 떼기 reason = 3, 100퍼센트 reason = 4
    if (ClientSocket != INVALID_SOCKET)
    {
        RitualNoticePacket Packet;
        Packet.header = ritualEndHeader;
        Packet.size = sizeof(RitualNoticePacket);
        Packet.id = MyCharacter->my_ID;
        Packet.ritual_id = ritual_id;
        Packet.reason = reason;
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(RitualNoticePacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendEndRitual failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CloseConnection();
    }
}

void AMySocketCultistActor::ProcessRitualData(const char* Buffer) {
    RitualGagePacket Received;
    memcpy(&Received, Buffer, sizeof(RitualGagePacket));

    const uint8_t ritual_id = Received.ritual_id;
    const int gauge = Received.gauge;

    AsyncTask(ENamedThreads::GameThread, [this, ritual_id, gauge]() {
        // gauge으로 ritual gauge 수정
        });
}

void AMySocketCultistActor::ProcessRitualEnd(const char* Buffer) {
    RitualNoticePacket Received;
    memcpy(&Received, Buffer, sizeof(RitualNoticePacket));

    if (Received.reason == 4) {
        // 제단 100퍼센트 완료
        // 캐릭터 손 떼게 하고, 제단 100퍼센트로 수정
    }
    else {
        const uint8_t ritual_id = Received.ritual_id;
        const int gauge = Received.reason;
        AsyncTask(ENamedThreads::GameThread, [this, ritual_id, gauge]() {
            // gauge으로 ritual gauge 수정

            });
    }
}

void AMySocketCultistActor::HandleMontageSitNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
    if (!MyCharacter) return;
    if (NotifyName != TEXT("Sit")) return;

    if (UAnimInstance* Anim = BoundAnimInstance.Get())
    {
        Anim->OnPlayMontageNotifyBegin.RemoveDynamic(
            this, &AMySocketCultistActor::HandleMontageSitNotifyBegin);
        BoundAnimInstance.Reset();
    }

    if (USkeletalMeshComponent* Mesh = MyCharacter->GetMesh())
    {
        if (UAnimInstance* Anim = Mesh->GetAnimInstance())
        {
            if (MyCharacter->AS_WoundedSitting1_Montage)
            {
                Anim->Montage_Play(MyCharacter->AS_WoundedSitting1_Montage, 1.0f);
            }
        }
    }
}

void AMySocketCultistActor::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (!MyCharacter) return;

    if (Montage == MyCharacter->AS_BandageFriendSquat1_Montage)
    {
        MyCharacter->ABP_DoHeal = false;
        UE_LOG(LogTemp, Warning, TEXT("DoHeal Ended"));
    }
    else if (Montage == MyCharacter->AS_WoundedSitting1_Montage)
    {
        MyCharacter->ABP_GetHeal = false;
        UE_LOG(LogTemp, Warning, TEXT("GetHeal Ended"));
    }

    if (Montage == MyCharacter->AS_WoundedSitting1_Montage ||
        Montage == MyCharacter->AS_BandageFriendSquat1_Montage)
    {
        if (UAnimInstance* Anim = MyCharacter->GetMesh()->GetAnimInstance())
        {
            Anim->OnMontageEnded.RemoveDynamic(
                this, &AMySocketCultistActor::HandleMontageEnded);
        }
    }
}

void AMySocketCultistActor::SendDisconnection() {
    NoticePacket Packet;
    Packet.header = DisconnectionHeader;
    Packet.size = sizeof(NoticePacket);

    int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(NoticePacket), 0);
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendDisconnection failed with error: %ld"), WSAGetLastError());
    }

    closesocket(ClientSocket);
    ClientSocket = INVALID_SOCKET;

    WSACleanup();
    UE_LOG(LogTemp, Log, TEXT("Client socket closed and cleaned up."));
}

void AMySocketCultistActor::CloseConnection() {
    closesocket(ClientSocket);
    ClientSocket = INVALID_SOCKET;
    WSACleanup();

    AsyncTask(ENamedThreads::GameThread, [this]()
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

            // 게임 종료
            if (PC)
            {
                PC->bShowMouseCursor = true;
                PC->SetInputMode(FInputModeUIOnly());

                TSubclassOf<UUserWidget> GameResultWidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Cult_Custom/WBP_GameResult.WBP_GameResult_C"));
                if (GameResultWidgetClass)
                {
                    UUserWidget* GameResultWidget = CreateWidget<UUserWidget>(PC, GameResultWidgetClass);
                    if (GameResultWidget)
                    {
                        GameResultWidget->AddToViewport();

                        UTextBlock* ResultTextBlock = Cast<UTextBlock>(GameResultWidget->GetWidgetFromName(TEXT("TextBlock_ResultText")));
                        if (ResultTextBlock)
                        {
                            ResultTextBlock->SetText(FText::FromString(TEXT("GameOver"))); // 또는 Cultist Win
                        }
                    }
                }
            }
            this->Destroy();
        }
    );
}

void AMySocketCultistActor::SafeDestroyCharacter(int PlayerID)
{
    // 복사해서 쓰는 방식
    ACharacter* CharToDestroy = nullptr;

    if (ACharacter* const* FoundPtr = SpawnedCharacters.Find(PlayerID))
    {
        CharToDestroy = *FoundPtr;
    }

    if (!CharToDestroy || !IsValid(CharToDestroy) || !CharToDestroy->IsValidLowLevelFast())
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid character pointer for ID=%d"), PlayerID);
        return;
    }

    // GameThread에서만 Destroy 하도록
    AsyncTask(ENamedThreads::GameThread, [this, PlayerID, CharToDestroy]()
        {
            if (!IsValid(CharToDestroy) || CharToDestroy->IsPendingKillPending())
            {
                UE_LOG(LogTemp, Warning, TEXT("Character already pending destroy: %d"), PlayerID);
                return;
            }

            UE_LOG(LogTemp, Log, TEXT("Destroying character safely on GameThread for ID=%d"), PlayerID);

            CharToDestroy->Destroy();
            SpawnedCharacters.Remove(PlayerID);
            ReceivedCultistStates.Remove(PlayerID);
        });
}

/*
void AMySocketClientActor::InitializeBlocks()
{
    TArray<AActor*> FoundBlocks;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AReplicatedPhysicsBlock::StaticClass(), FoundBlocks);

    int32 BlockIndex = 1;
    for (AActor* Actor : FoundBlocks)
    {
        AReplicatedPhysicsBlock* Block = Cast<AReplicatedPhysicsBlock>(Actor);
        if (Block)
        {
            SyncedBlocks.Add(BlockIndex, Block);
            FTransform BlockTransform = Block->GetActorTransform();

            BlockIndex++;
        }
    }
}
*/
/*
void AMySocketClientActor::ProcessObjectData(char* Buffer, int32 BytesReceived) {
    int32 Offset = sizeof(uint8);

    if (BytesReceived < Offset + sizeof(int32) + sizeof(FTransform)) {
        UE_LOG(LogTemp, Error, TEXT("Invalid object data packet received."));
        return;
    }

    int32 BlockID;
    FTransform NewTransform;

    memcpy(&BlockID, Buffer + Offset, sizeof(int32));
    Offset += sizeof(int32);
    memcpy(&NewTransform, Buffer + Offset, sizeof(FTransform));

    LastReceivedTransform.FindOrAdd(BlockID) = NewTransform;
}
*/
/*
void AMySocketClientActor::ProcessObjectUpdates(float DeltaTime)
{
    for (auto& Pair : LastReceivedTransform)
    {
        int32 BlockID = Pair.Key;
        FTransform TargetTransform = Pair.Value;

        if (AReplicatedPhysicsBlock* Block = SyncedBlocks.FindRef(BlockID))
        {
            FVector InterpolatedLocation = FMath::VInterpTo(
                Block->GetActorLocation(), TargetTransform.GetLocation(), DeltaTime, 5.0f);

            FRotator InterpolatedRotation = FMath::RInterpTo(
                Block->GetActorRotation(), TargetTransform.GetRotation().Rotator(), DeltaTime, 5.0f);

            Block->SetActorLocation(InterpolatedLocation);
            Block->SetActorRotation(InterpolatedRotation);
        }
    }
}
*/
// Called every frame
void AMySocketCultistActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    SendPlayerData();
    ProcessCharacterUpdates();
    // ProcessObjectUpdates(DeltaTime);
}

