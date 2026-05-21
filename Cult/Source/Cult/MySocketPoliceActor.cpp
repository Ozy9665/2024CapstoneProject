// Fill out your copyright notice in the Description page of Project Settings.


#include "MySocketPoliceActor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include "Camera/CameraActor.h"
#include "PoliceCharacter.h"
#include "Components/TextBlock.h"
#include "TreeObstacleActor.h"
#include "CrowActor.h"
#include "StructGraphManager.h"
#include "Kismet/GameplayStatics.h"

#pragma comment(lib, "ws2_32.lib")
AMySocketPoliceActor* MySocketPoliceActor = nullptr;

// Sets default values
AMySocketPoliceActor::AMySocketPoliceActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AMySocketPoliceActor::BeginPlay()
{
	Super::BeginPlay();
    MySocketPoliceActor = this;
    MyCharacter = Cast<APoliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    if (!MyCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("My character not found!"));
    }
    GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) {
        UE_LOG(LogTemp, Error, TEXT("My Socket Police Actor GetGameInstance Failed!"));
    }

    InitializeSyncedObjects();
}

void AMySocketPoliceActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (ClientSocket != INVALID_SOCKET)
    {
        SendDisconnection();
    }
}

void AMySocketPoliceActor::SetClientSocket(SOCKET InSocket, int32 RoomNumber)
{
    ClientSocket = InSocket;
    if (ClientSocket != INVALID_SOCKET)
    {
        ReceiveData();
        UE_LOG(LogTemp, Log, TEXT("Police Client socket set. Starting ReceiveData."));

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

void AMySocketPoliceActor::LogAndCleanupSocketError(const TCHAR* ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("%s with error: %ld"), ErrorMessage, WSAGetLastError());
    if (ClientSocket != INVALID_SOCKET)
    {
        closesocket(ClientSocket);
        ClientSocket = INVALID_SOCKET;
    }
    WSACleanup();
}

void AMySocketPoliceActor::ReceiveData()
{
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
        {
            std::vector<char> PendingBuffer;
            PendingBuffer.reserve(4096);

            while (true)
            {
                char Buffer[BufferSize];
                int32 BytesReceived = recv(ClientSocket, Buffer, BufferSize, 0);
                if (BytesReceived > 0)
                {
                    PendingBuffer.insert(PendingBuffer.end(), Buffer, Buffer + BytesReceived);
                    while (true) 
                    {
                        if (PendingBuffer.size() < 3)
                            break;

                        uint8 PacketType = static_cast<uint8>(PendingBuffer[0]);
                        uint16 PacketSize;
                        memcpy(&PacketSize, PendingBuffer.data() + 1, sizeof(uint16));

                        if (PendingBuffer.size() < PacketSize)
                            break;

                        std::vector<char> OnePacket(PendingBuffer.begin(),
                            PendingBuffer.begin() + PacketSize);

                        PendingBuffer.erase(PendingBuffer.begin(),
                            PendingBuffer.begin() + PacketSize);

                        switch (PacketType)
                        {
                        case cultistHeader:
                            ProcessPlayerData(OnePacket.data());
                            break;
                        case treeHeader:
                            ProcessTreeData(OnePacket.data());
                            break;
                        case crowSpawnHeader:
                            ProcessCrowSpawnData(OnePacket.data());
                            break;
                        case crowDataHeader:
                            ProcessCrowData(OnePacket.data());
                            break;
                        case crowDisableHeader:
                            ProcessCrowDisable(OnePacket.data());
                            break;
                        case particleHeader:
                            //ProcessParticleData(OnePacket.data());
                            break;
                        case hitHeader:
                            //ProcessHitData(OnePacket.data());
                            break;
                        case connectionHeader:
                            ProcessConnection(OnePacket.data());
                            break;
                        case DisconnectionHeader:
                            ProcessDisconnection(OnePacket.data());
                            break;
                        case ritualStartHeader:
                            ProcessRitualStart(OnePacket.data());
                            break;
                        case ritualDataHeader:
                            ProcessRitualData(OnePacket.data());
                            break;
                        case ritualEndHeader:
                            ProcessRitualEnd(OnePacket.data());
                            break;
                        case disappearHeader:
                        {
                            unsigned char id = static_cast<unsigned char>(OnePacket[2]);
                            HideCharacter(id, true);
                            break;
                        }
                        case appearHeader:
                        {
                            unsigned char id = static_cast<unsigned char>(OnePacket[2]);
                            HideCharacter(id, false);
                            break;
                        }
                        case collapseHeader:
                            ProcessCollapse(OnePacket.data());
                            break;
                        case objectClaimHeader:
                            ProcessObjectClaim(OnePacket.data());
                            break;
                        case objectUpdateHeader:
                            ProcessObjectUpdate(OnePacket.data());
                            break;
                        case objectEndHeader:
                            ProcessObjectEnd(OnePacket.data());
                            break;
                        default:
                            UE_LOG(LogTemp, Warning, TEXT("Unknown packet type received: %d"), PacketType);
                            break;
                        }
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

void AMySocketPoliceActor::ProcessPlayerData(const char* Buffer)
{
    const CultistPacket* pkt = reinterpret_cast<const CultistPacket*>(Buffer);
    const FCultistCharacterState& ReceivedState = pkt->state;
    {
        FScopeLock Lock(&ReceivedDataMutex);
        ReceivedCultistStates.FindOrAdd(ReceivedState.PlayerID) = ReceivedState;
    }
}

void AMySocketPoliceActor::ProcessHitData(const char* Buffer)
{
    const HitResultPacket* ReceivedPacket = reinterpret_cast<const HitResultPacket*>(Buffer);
    UE_LOG(LogTemp, Error, TEXT("ProcessHitData error with attacker: %d"), ReceivedPacket->AttackerID);
}

void AMySocketPoliceActor::ProcessTreeData(const char* Buffer) 
{
    const TreePacket* ReceivedSkill = reinterpret_cast<const TreePacket*>(Buffer);
    const int Key = ReceivedSkill->casterId;
 
    ACharacter* FoundChar = SpawnedCharacters.FindRef(Key);
    if (!FoundChar) {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] caster %d not found"), Key);
        return;
    }

    ACultistCharacter* CasterCultist = Cast<ACultistCharacter>(FoundChar);
    if (!CasterCultist) {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] caster %d is not Cultist"), Key);
        return;
    }

    TWeakObjectPtr<ACultistCharacter> WeakCaster = CasterCultist;
    const FVector  SpawnLoc = AMySocketActor::ToUE(ReceivedSkill->SpawnLoc);
    const FRotator SpawnRot = AMySocketActor::ToUE(ReceivedSkill->SpawnRot);

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

void AMySocketPoliceActor::ProcessCrowSpawnData(const char* Buffer) 
{
    const Crow* ReceivedCrow = reinterpret_cast<const Crow*>(Buffer);
    const int Key = ReceivedCrow->owner;

    ACharacter* FoundChar = SpawnedCharacters.FindRef(Key);
    if (!FoundChar) {
        UE_LOG(LogTemp, Warning, TEXT("[CrowSpawn] caster %d not found"), Key);
        return;
    }

    ACultistCharacter* CasterCultist = Cast<ACultistCharacter>(FoundChar);
    if (!CasterCultist) {
        UE_LOG(LogTemp, Warning, TEXT("[CrowSpawn] caster %d is not Cultist"), Key);
        return;
    }

    TWeakObjectPtr<ACultistCharacter> WeakCaster = CasterCultist;
    const FVector  SpawnLoc = AMySocketActor::ToUE(ReceivedCrow->loc);
    const FRotator SpawnRot = AMySocketActor::ToUE(ReceivedCrow->rot);

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
            UE_LOG(LogTemp, Warning, TEXT("[CrowSpawn] Spawned crow for caster=%s"), *Caster->GetName());
        }
        });
}

void AMySocketPoliceActor::ProcessCrowData(const char* Buffer) 
{
    const Crow* ReceivedCrow = reinterpret_cast<const Crow*>(Buffer);
    const int Key = ReceivedCrow->owner;

    ACharacter* FoundChar = SpawnedCharacters.FindRef(Key);
    if (!FoundChar) {
        UE_LOG(LogTemp, Warning, TEXT("[CrowData] caster %d not found"), Key);
        return;
    }

    ACultistCharacter* CasterCultist = Cast<ACultistCharacter>(FoundChar);
    if (!CasterCultist) {
        UE_LOG(LogTemp, Warning, TEXT("[CrowData] caster %d is not Cultist"), Key);
        return;
    }
    if (CasterCultist->CrowInstance) {
        // 까마귀 업데이트
        AsyncTask(ENamedThreads::GameThread, [CI = CasterCultist->CrowInstance, ReceivedCrow]() {
            if (IsValid(CI)) {
                CI->SetActorLocation(AMySocketActor::ToUE(ReceivedCrow->loc));
                CI->SetActorRotation(AMySocketActor::ToUE(ReceivedCrow->rot));
            }
            });
    }
}

void AMySocketPoliceActor::ProcessCrowDisable(const char* Buffer) 
{
    const IdOnlyPacket* packet = reinterpret_cast<const IdOnlyPacket*>(Buffer);
    const int Key = packet->id;

    ACharacter* FoundChar = SpawnedCharacters.FindRef(Key);
    if (!FoundChar) {
        UE_LOG(LogTemp, Warning, TEXT("[CrowDisable] caster %d not found"), Key);
        return;
    }

    ACultistCharacter* CasterCultist = Cast<ACultistCharacter>(FoundChar);
    if (!CasterCultist) {
        UE_LOG(LogTemp, Warning, TEXT("[CrowDisable] caster %d is not Cultist"), Key);
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

void AMySocketPoliceActor::ProcessConnection(const char* Buffer) {
    const IdRolePacket* pkt = reinterpret_cast<const IdRolePacket*>(Buffer);
    const int connectedId = pkt->id;
    const uint8_t role = pkt->role;

    if (my_ID == -1) {
        my_ID = connectedId;
        UE_LOG(LogTemp, Warning, TEXT("Connected. My ID is: %d"), my_ID);

        if (MyCharacter) {
            MyCharacter->my_ID = my_ID;
        }
    }
    else {
        AsyncTask(ENamedThreads::GameThread, [this, connectedId, role]() mutable
            {
                if (role == 0 || role == 100) // Cultist
                {
                    this->SpawnCultistCharacter(connectedId);
                }
                else if (role == 1) // Police
                {
                    UE_LOG(LogTemp, Warning, TEXT("ProcessConnection called with role 1. ID:%d"), my_ID);
                    // this->SpawnPoliceCharacter(connectedId);
                }
            });
    }
}

void AMySocketPoliceActor::ProcessDisconnection(const char* Buffer)
{
    const IdOnlyPacket* pkt = reinterpret_cast<const IdOnlyPacket*>(Buffer);
    const int DisconnectedID = pkt->id;
    if (DisconnectedID == my_ID)
    {
        CloseConnection();
    }
    else {
        SafeDestroyCharacter(DisconnectedID);
    }
}

void AMySocketPoliceActor::ProcessRitualStart(const char* Buffer)
{
    const RitualNoticePacket* Received = reinterpret_cast<const RitualNoticePacket*>(Buffer);

    const uint8_t ritual_id = Received->ritual_id;
    const uint8_t reason = Received->reason;

    TWeakObjectPtr<AMySocketPoliceActor> WeakThis(this);

    AsyncTask(ENamedThreads::GameThread, [WeakThis, ritual_id, reason]()
        {
            AMySocketPoliceActor* Self = WeakThis.Get();
            if (!Self)
                return;

            UWorld* World = Self->GetWorld();
            if (!World)
                return;

            UE_LOG(LogTemp, Warning,
                TEXT("[ProcessRitualStart] recv ritual_id=%d reason=%d"),
                static_cast<int32>(ritual_id),
                static_cast<int32>(reason)
            );

            AAltar* TargetAltar = nullptr;
            int32 MatchCount = 0;

            TArray<AActor*> FoundAltars;
            UGameplayStatics::GetAllActorsOfClass(World, AAltar::StaticClass(), FoundAltars);

            for (AActor* Actor : FoundAltars)
            {
                AAltar* Altar = Cast<AAltar>(Actor);
                if (!Altar)
                    continue;

                UE_LOG(LogTemp, Warning,
                    TEXT("[AltarList] Name=%s AltarID=%d Location=%s"),
                    *Altar->GetName(),
                    Altar->AltarID,
                    *Altar->GetActorLocation().ToString()
                );

                if (Altar->AltarID == static_cast<int32>(ritual_id))
                {
                    ++MatchCount;

                    if (!TargetAltar)
                    {
                        TargetAltar = Altar;
                    }
                }
            }

            if (MatchCount > 1)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("[ProcessRitualStart] Duplicate AltarID detected. ritual_id=%d MatchCount=%d"),
                    static_cast<int32>(ritual_id),
                    MatchCount
                );
            }

            if (!TargetAltar)
            {
                UE_LOG(LogTemp, Error,
                    TEXT("[ProcessRitualStart] TargetAltar not found. ritual_id=%d"),
                    static_cast<int32>(ritual_id)
                );
                return;
            }

            UE_LOG(LogTemp, Warning,
                TEXT("[ProcessRitualStart] TargetAltar=%s AltarID=%d Location=%s"),
                *TargetAltar->GetName(),
                TargetAltar->AltarID,
                *TargetAltar->GetActorLocation().ToString()
            );

            if (reason == 0)
            {
                TargetAltar->StartRitualProgressFXFromServer();
            }
            else if (reason == 1)
            {
                TargetAltar->PlayQTESuccessFXFromServer();
            }
            else if (reason == 2)
            {
                TargetAltar->PlayQTEFailFXFromServer();
            }
        });
}

void AMySocketPoliceActor::ProcessRitualData(const char* Buffer)
{
    const RitualGagePacket* Received = reinterpret_cast<const RitualGagePacket*>(Buffer);
    const uint8_t ritual_id = Received->ritual_id;
    const int gauge = Received->gauge;

    AsyncTask(ENamedThreads::GameThread, [this, ritual_id, gauge]() {
        // gauge으로 ritual gauge 수정
        TArray<AActor*> FoundAltars;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAltar::StaticClass(), FoundAltars);

        for (AActor* Actor : FoundAltars)
        {
            AAltar* TargetAltar = Cast<AAltar>(Actor);

            if (TargetAltar && TargetAltar->AltarID == (int32)ritual_id)
            {
                TargetAltar->AddToRitualGauge((float)gauge);
                break;
            }
        }
        });
}

void AMySocketPoliceActor::ProcessRitualEnd(const char* Buffer) {
    const RitualNoticePacket* Received = reinterpret_cast<const RitualNoticePacket*>(Buffer);
    if (Received->reason == 4) {
        // 제단 100퍼센트 완료
        const uint8_t ritual_id = Received->ritual_id;
        const int reason = Received->reason;
        // 캐릭터 손 떼게 하고, 제단 100퍼센트로 수정

        TWeakObjectPtr<AMySocketPoliceActor> WeakThis(this);
        AsyncTask(ENamedThreads::GameThread, [WeakThis, ritual_id, reason]()
            {
                AMySocketPoliceActor* Self = WeakThis.Get();
                if (!Self)
                    return;

                UWorld* World = Self->GetWorld();
                if (!World)
                    return;

                // 추가부분
                TArray<AActor*> FoundAltars;
                UGameplayStatics::GetAllActorsOfClass(World, AAltar::StaticClass(), FoundAltars);

                for (AActor* Actor : FoundAltars)
                {
                    AAltar* TargetAltar = Cast<AAltar>(Actor);
                    if (!TargetAltar)
                        continue;

                    if (TargetAltar->AltarID == static_cast<int32>(ritual_id))
                    {
                        TargetAltar->AddToRitualGauge(100.0f);

                        UE_LOG(LogTemp, Warning, TEXT("[RitualEnd] Altar %d gauge to 100"), ritual_id);
                        break;
                    }
                }

                AActor* FoundActor = UGameplayStatics::GetActorOfClass(
                    World,
                    AStructGraphManager::StaticClass()
                );

                AStructGraphManager* StructGraphManager = Cast<AStructGraphManager>(FoundActor);
                if (!StructGraphManager)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Collapse] StructGraphManager not found"));
                    return;
                }

                StructGraphManager->TriggerStage3();
            });
    }
    else {
        const uint8_t ritual_id = Received->ritual_id;
        const int gauge = Received->reason;
        AsyncTask(ENamedThreads::GameThread, [this, ritual_id, gauge]() {
            AsyncTask(ENamedThreads::GameThread, [this, ritual_id, gauge]() {
                // gauge로 ritual gauge
                TArray<AActor*> FoundAltars;
                UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAltar::StaticClass(), FoundAltars);

                for (AActor* Actor : FoundAltars)
                {
                    AAltar* TargetAltar = Cast<AAltar>(Actor);

                    if (TargetAltar && TargetAltar->AltarID == (int32)ritual_id)
                    {
                        TargetAltar->AddToRitualGauge((float)gauge);
                        break;
                    }
                }
                });
            });
    }
}

void AMySocketPoliceActor::ProcessCollapse(const char* Buffer)
{
    const CollapsePacket* pkt = reinterpret_cast<const CollapsePacket*>(Buffer);

    if (!pkt)
        return;

    if (pkt->size != sizeof(CollapsePacket))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Collapse] Invalid packet size"));
        return;
    }

    TWeakObjectPtr<AMySocketPoliceActor> WeakThis(this);

    AsyncTask(ENamedThreads::GameThread, [WeakThis]()
        {
            AMySocketPoliceActor* Self = WeakThis.Get();
            if (!Self)
                return;

            UWorld* World = Self->GetWorld();
            if (!World)
                return;

            AActor* FoundActor = UGameplayStatics::GetActorOfClass(
                World,
                AStructGraphManager::StaticClass()
            );

            AStructGraphManager* StructGraphManager = Cast<AStructGraphManager>(FoundActor);
            if (!StructGraphManager)
            {
                UE_LOG(LogTemp, Warning, TEXT("[Collapse] StructGraphManager not found"));
                return;
            }

            StructGraphManager->TriggerStage3();
        });
}

void AMySocketPoliceActor::SendPlayerData()
{
    if (ClientSocket != INVALID_SOCKET)
    {
        PolicePacket Packet;
        Packet.header = policeHeader;
        Packet.size = sizeof(PolicePacket);
        Packet.state = GetCharacterState();
        
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(PolicePacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendPlayerData failed with error: %ld"), WSAGetLastError());
        }
        SendDogData();
    }
    else
    {
        CloseConnection();
    }
}

void AMySocketPoliceActor::SendDogData() {
    if (!MyCharacter || !MyCharacter->PoliceDogInstance) {
        UE_LOG(LogTemp, Error, TEXT("No PoliceDogInstance "));
        return;
    }
    if (ClientSocket != INVALID_SOCKET)
    {
        DogPacket Packet;
        Packet.header = dogHeader;
        Packet.size = sizeof(DogPacket);
        Packet.dog = GetDog();
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(DogPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendDogData failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CloseConnection();
    }
}

void AMySocketPoliceActor::SendHitData(HitPacket Packet) {
    if (ClientSocket != INVALID_SOCKET)
    {
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(HitPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendPlayerData failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CloseConnection();
    }
}

FPoliceCharacterState AMySocketPoliceActor::GetCharacterState()
{
    FPoliceCharacterState State;
    State.PlayerID = MyCharacter->my_ID;

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
    State.bIsCrouching = MyCharacter->bIsCrouched;

    // Aiming 상태
    State.bIsAiming = MyCharacter->bIsAiming;

    // IsAttacking 상태
    State.bIsAttacking = MyCharacter->bIsAttacking;

    // 무기 값
    State.CurrentWeapon = MyCharacter->CurrentWeapon;

    // 파쿠르 관련 상태
    State.bIsPakour = MyCharacter->IsPakour;
    State.CurrentVaultType = MyCharacter->CurrentVaultType;
    // IsShooting 상태
    State.bIsShooting = MyCharacter->bIsShooting;

    return State;
}

Dog AMySocketPoliceActor::GetDog() {
    Dog dog{};
    dog.owner = MyCharacter->my_ID;
    if (MyCharacter->PoliceDogInstance)
    {
        dog.loc = AMySocketActor::ToNet(MyCharacter->PoliceDogInstance->GetActorLocation());
        dog.rot = AMySocketActor::ToNet(MyCharacter->PoliceDogInstance->GetActorRotation());

        // 개 상태 추가
    }

    return dog;
}

void AMySocketPoliceActor::SpawnCultistCharacter(const unsigned char PlayerID)
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
        UE_LOG(LogTemp, Error, TEXT("GI or PoliceClientClass is null"));
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
        UE_LOG(LogTemp, Log, TEXT("Spawned new character for PlayerID=%d"), PlayerID);

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

void AMySocketPoliceActor::ProcessCharacterUpdates()
{
    FScopeLock Lock(&ReceivedDataMutex);
    for (auto& Pair : ReceivedCultistStates)
    {
        const int PlayerID = Pair.Key;
        const FCultistCharacterState& State = Pair.Value;

        if (ACharacter* FoundChar = SpawnedCharacters.FindRef(PlayerID))
        {
            if (ACultistCharacter* CultistChar = Cast<ACultistCharacter>(FoundChar))
            {
                UpdateCultistState(CultistChar, State);
            }
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("No PlayerID %d"), PlayerID);
            KeysToRemove.Add(PlayerID);
        }
    }
    for (int32 Key : KeysToRemove)
    {
        ReceivedCultistStates.Remove(Key);
    }
    KeysToRemove.Reset();
}

void AMySocketPoliceActor::UpdateCultistState(ACharacter* Character, const FCultistCharacterState& State)
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

void AMySocketPoliceActor::UpdateCultistAnimInstanceProperties(UAnimInstance* AnimInstance, const FCultistCharacterState& State)
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

void AMySocketPoliceActor::CheckImpactEffect()
{
    // ImpactLocations.Add(MyCharacter->ParticleResult.ImpactPoint);
    UE_LOG(LogTemp, Log, TEXT("Added ImpactLocation: %s"), *MyCharacter->ParticleResult.ImpactPoint.ToString());
    SpawnImpactEffect(MyCharacter->ParticleResult);
    if (ClientSocket != INVALID_SOCKET)
    {
        SendParticleData(MyCharacter->ParticleResult);
    }
    else
    {
        CloseConnection();
    }
    MyCharacter->bHit = false;
    
}

void AMySocketPoliceActor::SpawnImpactEffect(FHitResult HitResult) {
    if (GI->NGParticleAsset)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), GI->NGParticleAsset,
            HitResult.ImpactPoint,
            HitResult.ImpactNormal.Rotation(),
            FVector(1.0f), true, true,
            ENCPoolMethod::None, true
        );
    }

    if (GI->MuzzleEffect)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), GI->MuzzleEffect,
            MyCharacter->MuzzleLocation->GetComponentLocation(),
            MyCharacter->MuzzleLocation->GetComponentRotation()
        );
    }
}

void AMySocketPoliceActor::SendParticleData(FHitResult HitResult) {
    FVector MuzzleLoc = MyCharacter->MuzzleLocation->GetComponentLocation();
    FRotator MuzzleRot = MyCharacter->MuzzleLocation->GetComponentRotation();

    ParticlePacket Packet;
    Packet.header = particleHeader;
    Packet.size = sizeof(ParticlePacket);
    Packet.data.ImpactX = HitResult.ImpactPoint.X;
    Packet.data.ImpactY = HitResult.ImpactPoint.Y;
    Packet.data.ImpactZ = HitResult.ImpactPoint.Z;
    Packet.data.NormalX = HitResult.ImpactNormal.X;
    Packet.data.NormalY = HitResult.ImpactNormal.Y;
    Packet.data.NormalZ = HitResult.ImpactNormal.Z;
    Packet.data.MuzzleX = MuzzleLoc.X;
    Packet.data.MuzzleY = MuzzleLoc.Y;
    Packet.data.MuzzleZ = MuzzleLoc.Z;
    Packet.data.MuzzlePitch = MuzzleRot.Pitch;
    Packet.data.MuzzleYaw = MuzzleRot.Yaw;
    Packet.data.MuzzleRoll = MuzzleRot.Roll;

    int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(ParticlePacket), 0);
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendParticleData failed with error: %ld"), WSAGetLastError());
    }
}

void AMySocketPoliceActor::HideCharacter(int PlayerID, bool bHide) {
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

void AMySocketPoliceActor::SendQuit() {
    NoticePacket Packet;
    Packet.header = quitHeader;
    Packet.size = sizeof(NoticePacket);

    int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(NoticePacket), 0);
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendDisconnection failed with error: %ld"), WSAGetLastError());
    }
}

void AMySocketPoliceActor::SendDisconnection() {
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

void AMySocketPoliceActor::CloseConnection() {
    closesocket(ClientSocket);
    ClientSocket = INVALID_SOCKET;
    WSACleanup();

    AsyncTask(ENamedThreads::GameThread, [this]()
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

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

void AMySocketPoliceActor::SafeDestroyCharacter(int PlayerID)
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

const TMap<int, ACharacter*>& AMySocketPoliceActor::GetSpawnedCharacters() const
{
    return SpawnedCharacters;
}

bool AMySocketPoliceActor::IsLocalOwnedObject(int ObjectID) const
{
    return LocalOwnedObjectIDs.Contains(ObjectID);
}

void AMySocketPoliceActor::AddLocalOwnedObject(int ObjectID)
{
    LocalOwnedObjectIDs.Add(ObjectID);
    LocalOwnedObjectStopTimers.FindOrAdd(ObjectID) = 0.0f;

    UE_LOG(LogTemp, Warning, TEXT("[ObjectOwner] Add LocalOwnedObject ID=%d Count=%d"),
        ObjectID,
        LocalOwnedObjectIDs.Num()
    );
}

void AMySocketPoliceActor::RemoveLocalOwnedObject(int ObjectID)
{
    LocalOwnedObjectIDs.Remove(ObjectID);
    LocalOwnedObjectStopTimers.Remove(ObjectID);

    UE_LOG(LogTemp, Warning, TEXT("[ObjectOwner] Remove LocalOwnedObject ID=%d Count=%d"),
        ObjectID,
        LocalOwnedObjectIDs.Num()
    );
}

void AMySocketPoliceActor::UpdateLocalOwnedObjects(float DeltaTime)
{
    TArray<ObjectUpdateData> Updates;
    TArray<int> EndObjects;

    for (int ObjectID : LocalOwnedObjectIDs)
    {
        AActor* ObjectActor = SyncedObjectActors.FindRef(ObjectID);
        if (!ObjectActor)
            continue;

        UStaticMeshComponent* MeshComp = ObjectActor->FindComponentByClass<UStaticMeshComponent>();
        if (!MeshComp)
            continue;

        const FVector Velocity = MeshComp->GetPhysicsLinearVelocity();
        const float Speed = Velocity.Size();

        float& StopTimer = LocalOwnedObjectStopTimers.FindOrAdd(ObjectID);

        if (Speed <= StopSpeedThreshold)
        {
            StopTimer += DeltaTime;
        }
        else
        {
            StopTimer = 0.0f;
        }

        if (StopTimer >= StopTimeThreshold)
        {
            SendObjectMoveEnd(
                ObjectID,
                ObjectActor->GetActorLocation(),
                ObjectActor->GetActorRotation()
            );

            EndObjects.Add(ObjectID);
            continue;
        }

        ObjectUpdateData Data{};
        Data.object_id = ObjectID;
        Data.loc = AMySocketActor::ToNet(ObjectActor->GetActorLocation());
        Data.rot = AMySocketActor::ToNet(ObjectActor->GetActorRotation());

        Updates.Add(Data);
    }

    if (!Updates.IsEmpty())
    {
        SendObjectUpdatePacket(Updates);
    }

    for (int ObjectID : EndObjects)
    {
        RemoveLocalOwnedObject(ObjectID);
    }
}

void AMySocketPoliceActor::SendObjectOwnerClaim(int ObjectID)
{
    ObjectOwnerClaimPacket Packet;
    Packet.header = objectClaimHeader;
    Packet.size = sizeof(ObjectOwnerClaimPacket);
    Packet.object_id = ObjectID;

    int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(ObjectOwnerClaimPacket), 0);
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendObjectOwnerClaim failed with error: %ld"), WSAGetLastError());
    }
}

void AMySocketPoliceActor::SendObjectUpdatePacket(const TArray<ObjectUpdateData>& Updates)
{
    if (ClientSocket == INVALID_SOCKET)
        return;

    if (Updates.IsEmpty())
        return;

    ObjectUpdatePacket Header{};
    Header.header = objectUpdateHeader;
    Header.count = static_cast<uint16_t>(Updates.Num());
    Header.size = static_cast<uint16_t>(
        sizeof(ObjectUpdatePacket) + sizeof(ObjectUpdateData) * Updates.Num()
        );

    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(Header.size);

    int32 Offset = 0;

    FMemory::Memcpy(Buffer.GetData() + Offset, &Header, sizeof(ObjectUpdatePacket));
    Offset += sizeof(ObjectUpdatePacket);

    FMemory::Memcpy(
        Buffer.GetData() + Offset,
        Updates.GetData(),
        sizeof(ObjectUpdateData) * Updates.Num()
    );

    int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(Buffer.GetData()), Buffer.Num(), 0);
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendObjectUpdatePacket failed. Error=%ld"), WSAGetLastError());
    }
}

void AMySocketPoliceActor::SendObjectMoveEnd(int ObjectID, const FVector& Loc, const FRotator& Rot)
{
    ObjectMoveEndPacket Packet;
    Packet.header = objectEndHeader;
    Packet.size = sizeof(ObjectMoveEndPacket);
    Packet.object_id = ObjectID;
    Packet.loc = AMySocketActor::ToNet(Loc);
    Packet.rot = AMySocketActor::ToNet(Rot);

    int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(ObjectMoveEndPacket), 0);
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendObjectMoveEnd failed with error: %ld"), WSAGetLastError());
    }
}

int AMySocketPoliceActor::GetObjectIDByActor(AActor* Actor) const
{
    if (!Actor)
        return -1;

    const int* ObjectID = SyncedObjectActorToID.Find(Actor);
    if (!ObjectID)
        return -1;

    return *ObjectID;
}

void AMySocketPoliceActor::InitializeSyncedObjects()
{
    SyncedObjectActors.Empty();
    SyncedObjectActorToID.Empty();

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(
        GetWorld(),
        AActor::StaticClass(),
        FoundActors
    );

    TArray<AActor*> SyncActors;

    for (AActor* Actor : FoundActors)
    {
        if (!Actor)
            continue;

        if (!Actor->ActorHasTag(TEXT("SyncObject")))
            continue;

        SyncActors.Add(Actor);
    }

    SyncActors.Sort([](const AActor& A, const AActor& B)
        {
            const FVector LA = A.GetActorLocation();
            const FVector LB = B.GetActorLocation();

            if (!FMath::IsNearlyEqual(LA.X, LB.X))
                return LA.X < LB.X;

            if (!FMath::IsNearlyEqual(LA.Y, LB.Y))
                return LA.Y < LB.Y;

            return LA.Z < LB.Z;
        });

    for (int i = 0; i < SyncActors.Num(); ++i)
    {
        AActor* Actor = SyncActors[i];
        if (!Actor)
            continue;

        const int ObjectID = i;

        SyncedObjectActors.Add(ObjectID, Actor);
        SyncedObjectActorToID.Add(Actor, ObjectID);
    }

    UE_LOG(LogTemp, Warning, TEXT("[ObjectSync] Total Registered=%d"), SyncedObjectActors.Num());
}

void AMySocketPoliceActor::RequestObjectOwnerClaim(int ObjectID)
{
    if (ObjectID < 0)
        return;

    if (IsLocalOwnedObject(ObjectID))
        return;

    AddLocalOwnedObject(ObjectID);
    SendObjectOwnerClaim(ObjectID);
}

void AMySocketPoliceActor::ProcessObjectClaim(const char* Buffer)
{
    const ObjectOwnerClaimPacket* Packet = reinterpret_cast<const ObjectOwnerClaimPacket*>(Buffer);

    if (Packet->size != sizeof(ObjectOwnerClaimPacket))
        return;

    const int ObjectID = Packet->object_id;

    if (IsLocalOwnedObject(ObjectID))
    {
        RemoveLocalOwnedObject(ObjectID);

        UE_LOG(LogTemp, Warning, TEXT("[ObjectSync] Claim received. Lost ownership ID=%d"), ObjectID);
    }
}

void AMySocketPoliceActor::ProcessObjectUpdate(const char* Buffer)
{
    const ObjectUpdatePacket* Packet =
        reinterpret_cast<const ObjectUpdatePacket*>(Buffer);

    if (Packet->count == 0)
        return;

    const int ExpectedSize =
        sizeof(ObjectUpdatePacket) + sizeof(ObjectUpdateData) * Packet->count;

    if (Packet->size != ExpectedSize)
        return;

    TArray<ObjectUpdateData> Updates;
    Updates.Reserve(Packet->count);

    int Offset = sizeof(ObjectUpdatePacket);

    for (int i = 0; i < Packet->count; ++i)
    {
        const ObjectUpdateData* Data =
            reinterpret_cast<const ObjectUpdateData*>(Buffer + Offset);

        Offset += sizeof(ObjectUpdateData);

        Updates.Add(*Data);
    }

    AsyncTask(ENamedThreads::GameThread, [this, Updates]()
        {
            for (const ObjectUpdateData& Data : Updates)
            {
                const int ObjectID = Data.object_id;

                if (IsLocalOwnedObject(ObjectID))
                    continue;

                AActor* ObjectActor = SyncedObjectActors.FindRef(ObjectID);
                if (!ObjectActor)
                    continue;

                const FVector NewLoc(
                    static_cast<float>(Data.loc.x),
                    static_cast<float>(Data.loc.y),
                    static_cast<float>(Data.loc.z)
                );

                const FRotator NewRot(
                    static_cast<float>(Data.rot.pitch),
                    static_cast<float>(Data.rot.yaw),
                    static_cast<float>(Data.rot.roll)
                );

                ObjectActor->SetActorLocationAndRotation(
                    NewLoc,
                    NewRot,
                    false,
                    nullptr,
                    ETeleportType::TeleportPhysics
                );
            }
        });
}

void AMySocketPoliceActor::ProcessObjectEnd(const char* Buffer)
{
    const ObjectMoveEndPacket* Packet =
        reinterpret_cast<const ObjectMoveEndPacket*>(Buffer);

    if (Packet->size != sizeof(ObjectMoveEndPacket))
        return;

    const ObjectMoveEndPacket PacketCopy = *Packet;

    AsyncTask(ENamedThreads::GameThread, [this, PacketCopy]()
        {
            const int ObjectID = PacketCopy.object_id;

            AActor* ObjectActor = SyncedObjectActors.FindRef(ObjectID);
            if (!ObjectActor)
                return;

            const FVector NewLoc(
                static_cast<float>(PacketCopy.loc.x),
                static_cast<float>(PacketCopy.loc.y),
                static_cast<float>(PacketCopy.loc.z)
            );

            const FRotator NewRot(
                static_cast<float>(PacketCopy.rot.pitch),
                static_cast<float>(PacketCopy.rot.yaw),
                static_cast<float>(PacketCopy.rot.roll)
            );

            ObjectActor->SetActorLocationAndRotation(
                NewLoc,
                NewRot,
                false,
                nullptr,
                ETeleportType::TeleportPhysics
            );

            if (IsLocalOwnedObject(ObjectID))
            {
                RemoveLocalOwnedObject(ObjectID);

                UE_LOG(LogTemp, Warning, TEXT("[ObjectSync] End received. Remove ownership ID=%d"), ObjectID);
            }
        });
}

// Called every frame
void AMySocketPoliceActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    SendPlayerData();
    ProcessCharacterUpdates();
    UpdateLocalOwnedObjects(DeltaTime);
}

