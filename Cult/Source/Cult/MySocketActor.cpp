// Fill out your copyright notice in the Description page of Project Settings.

#include "MySocketActor.h"
#include "Engine/Engine.h"
#include <GameFramework/Character.h>
#include "GameFramework/CharacterMovementComponent.h"
#include <Kismet/GameplayStatics.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")  // Winsock 라이브러리 링크

// Sets default values
AMySocketActor::AMySocketActor()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    ServerSocket = INVALID_SOCKET;

    ConstructorHelpers::FObjectFinder<UParticleSystem> ParticleAsset(TEXT("ParticleSystem'/Engine/Tutorial/SubEditors/TutorialAssets/TutorialParticleSystem.TutorialParticleSystem'"));
    if (ParticleAsset.Succeeded())
    {
        ImpactParticle = ParticleAsset.Object;
        UE_LOG(LogTemp, Log, TEXT("Successfully loaded ImpactParticle from code!"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load ImpactParticle in code! Check path."));
        ImpactParticle = nullptr;
    }
}

// Called when the game starts or when spawned
void AMySocketActor::BeginPlay()
{
    Super::BeginPlay();

    ServerCharacter = Cast<APoliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    if (!ServerCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("Server character not found!"));
    }
    InitializeBlocks();
    if (InitializeServer(7777))
    {
        UE_LOG(LogTemp, Log, TEXT("Server initialized and waiting for clients..."));
        AcceptClientAsync();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to initialize server."));
    }
}

void AMySocketActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    bIsRunning = false;

    CloseAllClientSockets();
    CloseServerSocket();

    WSACleanup();

    UE_LOG(LogTemp, Log, TEXT("All sockets closed and cleaned up."));
}

bool AMySocketActor::InitializeServer(int32 Port)
{
    WSADATA WsaData;
    int Result = WSAStartup(MAKEWORD(2, 2), &WsaData);
    if (Result != 0)
    {
        UE_LOG(LogTemp, Error, TEXT("WSAStartup failed: %d"), Result);
        return false;
    }

    ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ServerSocket == INVALID_SOCKET)
    {
        LogAndCleanupSocketError(TEXT("Failed to create socket"));
        return false;
    }

    sockaddr_in ServerAddr;
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port = htons(Port);

    if (bind(ServerSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)) == SOCKET_ERROR)
    {
        LogAndCleanupSocketError(TEXT("Bind failed"));
        return false;
    }

    if (listen(ServerSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        LogAndCleanupSocketError(TEXT("Listen failed"));
        return false;
    }

    return true;
}

void AMySocketActor::LogAndCleanupSocketError(const TCHAR* ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("%s with error: %ld"), ErrorMessage, WSAGetLastError());
    if (ServerSocket != INVALID_SOCKET)
    {
        closesocket(ServerSocket);
        ServerSocket = INVALID_SOCKET;
    }
    WSACleanup();
}

void AMySocketActor::AcceptClientAsync()
{
    bIsRunning = true;

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
        {
            while (bIsRunning)
            {
                SOCKET NewClientSocket = accept(ServerSocket, nullptr, nullptr);
                if (NewClientSocket == INVALID_SOCKET)
                {
                    UE_LOG(LogTemp, Error, TEXT("Accept failed with error: %ld"), WSAGetLastError());
                    break;
                }

                {
                    FScopeLock Lock(&ClientSocketsMutex);
                    ClientSockets.Add(NewClientSocket);
                }

                UE_LOG(LogTemp, Log, TEXT("New client connected! Socket: %d"), NewClientSocket);

                // 클라이언트별로 데이터를 비동기 수신
                ReceiveData(NewClientSocket);
            }
        });
}

void AMySocketActor::InitializeBlocks()
{
    TArray<AActor*> FoundBlocks;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AReplicatedPhysicsBlock::StaticClass(), FoundBlocks);

    int32 BlockIndex = 1;
    for (AActor* Actor : FoundBlocks)
    {
        AReplicatedPhysicsBlock* Block = Cast<AReplicatedPhysicsBlock>(Actor);
        if (Block)
        {
            Block->SetOwner(this);
            Block->SetBlockID(BlockIndex);
            BlockMap.Add(BlockIndex, Block);
            BlockTransforms.Add(BlockIndex, Block->GetActorTransform());
            UE_LOG(LogTemp, Error, TEXT("Added Block: ID=%d"), BlockIndex);
            BlockIndex++;
        }
    }
}

void AMySocketActor::SendCultistData(SOCKET TargetSocket)
{
    FScopeLock Lock(&ClientSocketsMutex);

    TArray<FCultistCharacterState> AllCharacterStates;
    AllCharacterStates.Reserve(ClientStates.Num());

    // 다른 클라이언트들의 캐릭터 상태 추가
    for (auto& Entry : ClientStates)
    {
        if (Entry.Key != TargetSocket)
        {
            AllCharacterStates.Add(Entry.Value);
        }
    }

    int32 TotalBytes = sizeof(uint8) + (AllCharacterStates.Num() * sizeof(FCultistCharacterState));
    TArray<uint8> PacketData;
    PacketData.SetNumUninitialized(TotalBytes);

    memcpy(PacketData.GetData(), &cultistHeader, sizeof(uint8));
    memcpy(PacketData.GetData() + sizeof(uint8), AllCharacterStates.GetData(), AllCharacterStates.Num() * sizeof(FCultistCharacterState));

    int BytesSent = send(TargetSocket, reinterpret_cast<const char*>(PacketData.GetData()), TotalBytes, 0);

    // 로그 추가
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendCultistData failed with WSAGetLastError %ld during send to %d"), WSAGetLastError(), TargetSocket);
        CloseClientSocket(TargetSocket);
    }
}

void AMySocketActor::SendPoliceData(SOCKET TargetSocket)
{
    FScopeLock Lock(&ClientSocketsMutex);

    FPoliceCharacterState PoliceState = GetServerCharacterState();

    int32 TotalBytes = sizeof(uint8) + sizeof(FPoliceCharacterState);
    TArray<uint8> PacketData;
    PacketData.SetNumUninitialized(TotalBytes);

    memcpy(PacketData.GetData(), &policeHeader, sizeof(uint8));
    memcpy(PacketData.GetData() + sizeof(uint8), &PoliceState, sizeof(FPoliceCharacterState));

    int BytesSent = send(TargetSocket, reinterpret_cast<const char*>(PacketData.GetData()), TotalBytes, 0);

    // 로그 추가
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendPoliceData failed with WSAGetLastError %ld during send to %d"), WSAGetLastError(), TargetSocket);
        CloseClientSocket(TargetSocket);
    }
}

void AMySocketActor::SendObjectData(int32 BlockID, FTransform NewTransform)
{
    FScopeLock Lock(&ClientSocketsMutex);

    if (BlockMap.Contains(BlockID)) // 유효한 블록인지 확인
    {
        int32 TotalBytes = sizeof(uint8) + sizeof(int32) + sizeof(FTransform);
        TArray<uint8> PacketData;
        PacketData.SetNumUninitialized(TotalBytes);

        memcpy(PacketData.GetData(), &objectHeader, sizeof(uint8));
        memcpy(PacketData.GetData() + sizeof(uint8), &BlockID, sizeof(int32));
        memcpy(PacketData.GetData() + sizeof(uint8) + sizeof(int32), &NewTransform, sizeof(FTransform));

        for (SOCKET ClientSocket : ClientSockets)
        {
            if (ClientSocket != INVALID_SOCKET)
            {
                int BytesSent = send(ClientSocket, reinterpret_cast<const char*>(PacketData.GetData()), TotalBytes, 0);

                // 오류 확인 및 로그 추가
                if (BytesSent == SOCKET_ERROR)
                {
                    UE_LOG(LogTemp, Error, TEXT("SendPlayerData failed with WSAGetLastError %ld during send to %d"), WSAGetLastError(), ClientSocket);
                    CloseClientSocket(ClientSocket);
                }
                else
                {
                    UE_LOG(LogTemp, Log, TEXT("Sent Player Data to %d"), ClientSocket);
                }
            }
        }
    }
}

void AMySocketActor::SendParticleData(SOCKET TargetSocket, FVector ImpactLoc) {
    int32 TotalBytes = sizeof(uint8) + sizeof(FVector);
    TArray<uint8> PacketData;
    PacketData.SetNumUninitialized(TotalBytes);

    memcpy(PacketData.GetData(), &particleHeader, sizeof(uint8));
    memcpy(PacketData.GetData() + sizeof(uint8), &ImpactLoc, sizeof(FVector));

    int BytesSent = send(TargetSocket, reinterpret_cast<const char*>(PacketData.GetData()), TotalBytes, 0);

    // 로그 추가
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendParticleData failed with WSAGetLastError %ld during send to %d"), WSAGetLastError(), TargetSocket);
        CloseClientSocket(TargetSocket);
    }
}

FPoliceCharacterState AMySocketActor::GetServerCharacterState()
{
    FPoliceCharacterState State;
    State.PlayerID = -1;

    // 위치 및 회전 설정
    State.PositionX = ServerCharacter->GetActorLocation().X;
    State.PositionY = ServerCharacter->GetActorLocation().Y;
    State.PositionZ = ServerCharacter->GetActorLocation().Z;
    State.RotationPitch = ServerCharacter->GetActorRotation().Pitch;
    State.RotationYaw = ServerCharacter->GetActorRotation().Yaw;
    State.RotationRoll = ServerCharacter->GetActorRotation().Roll;

    // 속도 및 Speed 계산
    FVector Velocity = ServerCharacter->GetVelocity();
    State.VelocityX = Velocity.X;
    State.VelocityY = Velocity.Y;
    State.VelocityZ = Velocity.Z;
    State.Speed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

    // Crouch 상태
    State.bIsCrouching = ServerCharacter->bIsCrouched;

    // Aiming 상태
    State.bIsAiming = ServerCharacter->bIsAiming;

    // IsAttacking 상태
    State.bIsAttacking = ServerCharacter->bIsAttacking;

    // 무기 값
    State.CurrentWeapon = ServerCharacter->CurrentWeapon;

    // 파쿠르 관련 상태
    State.bIsPakour = ServerCharacter->IsPakour;
    State.bIsNearEnoughToPakour = ServerCharacter->bIsNearEnoughToPakour;
    State.CurrentVaultType = ServerCharacter->CurrentVaultType;

    // IsShooting 상태
    State.bIsShooting = ServerCharacter->bIsShooting;

    return State;
}

void AMySocketActor::ReceiveData(SOCKET ClientSocket)
{
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, ClientSocket]()
        {
            while (true)
            {
                const int32 BufferSize = 1024; // 충분한 버퍼 크기 설정
                char Buffer[BufferSize];

                int32 BytesReceived = recv(ClientSocket, Buffer, BufferSize, 0);

                if (BytesReceived > 0)
                {
                    // 패킷의 첫 바이트는 헤더, 이후는 데이터
                    uint8 PacketType;
                    memcpy(&PacketType, Buffer, sizeof(uint8));

                    if (PacketType == cultistHeader)
                    {
                        ProcessPlayerData(ClientSocket, Buffer, BytesReceived);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Unknown packet type received: %d"), PacketType);
                    }
                }
                else if (BytesReceived == 0 || WSAGetLastError() == WSAECONNRESET)
                {
                    UE_LOG(LogTemp, Log, TEXT("Connection closed by server."));
                    break;
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("recv failed with error: %ld"), WSAGetLastError());
                    break;
                }
            }
        });
}

void AMySocketActor::ProcessPlayerData(SOCKET ClientSocket, char* Buffer, int32 BytesReceived)
{
    if (BytesReceived < sizeof(uint8) + sizeof(FCultistCharacterState))
    {
        UE_LOG(LogTemp, Warning, TEXT("Received player data packet size mismatch."));
        return;
    }

    FCultistCharacterState ReceivedState;
    FMemory::Memcpy(&ReceivedState, Buffer + sizeof(uint8), sizeof(FCultistCharacterState));

    ReceivedState.PlayerID = static_cast<int32>(ClientSocket);

    AsyncTask(ENamedThreads::GameThread, [this, ClientSocket, ReceivedState]()
        {
            FScopeLock Lock(&ClientSocketsMutex);
            ClientStates.FindOrAdd(ClientSocket) = ReceivedState;
            SpawnOrUpdateClientCharacter(ClientSocket, ReceivedState);
        });
}

void AMySocketActor::SpawnClientCharacter(SOCKET ClientSocket, const FCultistCharacterState& State)
{
    int32 AssignedPlayerID = ClientCharacters.Num() + 1; // 기존 클라이언트 수를 기반으로 ID 할당

    UE_LOG(LogTemp, Error, TEXT("Spawning Character for PlayerID: %d (Socket: %d)"), AssignedPlayerID, ClientSocket);

    if (ClientCharacters.Contains(ClientSocket))
    {
        UE_LOG(LogTemp, Warning, TEXT("Character already exists for Socket: %d"), ClientSocket);
        return;
    }

    // 새로운 클라이언트 캐릭터 생성
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // 각 클라이언트에 대한 고유 위치 생성
    FVector SpawnLocation = FVector((AssignedPlayerID % 10) * 200.0f, (AssignedPlayerID / 10) * 200.0f, 100.0f);

    UClass* BP_ClientCharacter = LoadClass<ACharacter>(nullptr, TEXT("/Game/Cult_Custom/Characters/BP_Cultist_A_Client.BP_Cultist_A_Client_C"));
    if (BP_ClientCharacter)
    {
        ACharacter* NewCharacter = GetWorld()->SpawnActor<ACharacter>(
            BP_ClientCharacter, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
        if (NewCharacter)
        {
            ClientCharacters.Add(ClientSocket, NewCharacter);

            UE_LOG(LogTemp, Log, TEXT("Client character spawned for PlayerID=%d at location."), AssignedPlayerID);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn client character for socket %d"), ClientSocket);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load BP_ClientCharacter class."));
    }
}

void AMySocketActor::SpawnOrUpdateClientCharacter(SOCKET ClientSocket, const FCultistCharacterState& State)
{
    AsyncTask(ENamedThreads::GameThread, [this, ClientSocket, State]()
        {
            if (State.CurrentHealth < 0)
            {
                UE_LOG(LogTemp, Warning, TEXT("PlayerID=%d is dead. Closing socket."), State.PlayerID);
                CloseClientSocket(ClientSocket);
                return;
            }

            if (ClientCharacters.Contains(ClientSocket))
            {
                // 기존 캐릭터 업데이트
                ACharacter* Character = ClientCharacters[ClientSocket];
                if (Character)
                {
                    UpdateCharacterState(Character, State);
                }
            }
            else
            {
                SpawnClientCharacter(ClientSocket, State);
            }
        });
}

void AMySocketActor::UpdateCharacterState(ACharacter* Character, const FCultistCharacterState& State)
{
    const float InterpSpeed = 30.0f; // 보간 속도 
    const float DeltaTime = GetWorld()->GetDeltaSeconds();

    FVector CurrentLocation = Character->GetActorLocation();
    FVector TargetLocation = FVector(State.PositionX, State.PositionY, State.PositionZ);

    if (State.bIsCrouching)
    {
        TargetLocation.Z += 50.0f;
    }

    FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, InterpSpeed);
    Character->SetActorLocation(NewLocation);
    Character->SetActorRotation(FRotator(State.RotationPitch, State.RotationYaw, State.RotationRoll));

    // 애니메이션 상태 업데이트 추가
    if (USkeletalMeshComponent* Mesh = Character->GetMesh())
    {
        UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
        if (AnimInstance)
        {
            UpdateAnimInstanceProperties(AnimInstance, State);
        }
    }
}

void AMySocketActor::UpdateAnimInstanceProperties(UAnimInstance* AnimInstance, const FCultistCharacterState& State)
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

    // IsCrouching 업데이트
    FProperty* IsCrouchingProperty = AnimInstance->GetClass()->FindPropertyByName(FName("Crouch"));
    if (IsCrouchingProperty && IsCrouchingProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsCrouchingProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, State.bIsCrouching);
    }

    FProperty* IsABP_IsPerformingProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsPerforming"));
    if (IsABP_IsPerformingProperty && IsABP_IsPerformingProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_IsPerformingProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, State.bIsPerformingRitual);
    }
    
    FProperty* IsABP_IsHitByAnAttackProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsHitByAnAttack"));
    if (IsABP_IsHitByAnAttackProperty && IsABP_IsHitByAnAttackProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_IsHitByAnAttackProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, State.bIsHitByAnAttack);
    }
}

void AMySocketActor::CheckImpactEffect()
{
    APoliceCharacter* PoliceChar = Cast<APoliceCharacter>(ServerCharacter);
    if (PoliceChar)
    {
        if (PoliceChar->bHit)
        {
            FVector ImpactLoc = PoliceChar->ImpactLoc;
            ImpactLocations.Add(ImpactLoc);
            SpawnImpactEffect(ImpactLoc);
            UE_LOG(LogTemp, Log, TEXT("Added ImpactLocation: %s"), *ImpactLoc.ToString());

            FScopeLock Lock(&ClientSocketsMutex);
            for (SOCKET ClientSocket : ClientSockets)
            {
                if (ClientSocket != INVALID_SOCKET)
                {
                    SendParticleData(ClientSocket, ImpactLoc);
                }
            }

            PoliceChar->bHit = false;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CheckImpactEffect: ServerCharacter is not a valid PoliceCharacter."));
    }
}

void AMySocketActor::SpawnImpactEffect(const FVector& ImpactLocation)
{
    if (ImpactParticle)
    {
        UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(), ImpactParticle, ImpactLocation, FRotator::ZeroRotator, true);
        if (PSC)
        {
            PSC->bAutoDestroy = true;
            // UE_LOG(LogTemp, Log, TEXT("Spawned Impact Effect at: %s"), *ImpactLocation.ToString());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ImpactParticle is not set."));
    }
}

void AMySocketActor::CloseClientSocket(SOCKET ClientSocket)
{
    AsyncTask(ENamedThreads::GameThread, [this, ClientSocket]()
        {
            FScopeLock Lock(&ClientSocketsMutex);
            /*if (ClientCharacters.Contains(ClientSocket))
            {
                ACharacter* Character = ClientCharacters[ClientSocket];
                if (Character)
                {
                    Character->Destroy();
                }
                ClientCharacters.Remove(ClientSocket);
            }
            if (ClientSockets.Contains(ClientSocket))
            {
                ClientSockets.Remove(ClientSocket);
            }*/
            int32 DisconnectedID = static_cast<int32>(ClientSocket);
            int32 PacketSize = sizeof(uint8) + sizeof(int32);

            TArray<uint8> Packet;
            Packet.SetNumUninitialized(PacketSize);
            memcpy(Packet.GetData(), &DisconnectionHeader, sizeof(uint8));
            memcpy(Packet.GetData() + sizeof(uint8), &DisconnectedID, sizeof(int32));

            for (SOCKET Socket : ClientSockets)
            {
                if (Socket != INVALID_SOCKET)
                {
                    send(Socket, reinterpret_cast<const char*>(Packet.GetData()), PacketSize, 0);
                }
            }

            ClientCharacters.Remove(ClientSocket);
            ClientStates.Remove(ClientSocket);
            ClientSockets.Remove(ClientSocket);
            closesocket(ClientSocket);

            UE_LOG(LogTemp, Error, TEXT("Closed client socket: %d"), ClientSocket);
        });
}

void AMySocketActor::CloseAllClientSockets()
{
    FScopeLock Lock(&ClientSocketsMutex);
    for (SOCKET& ClientSocket : ClientSockets)
    {
        if (ClientSocket != INVALID_SOCKET)
        {
            closesocket(ClientSocket);
            ClientSocket = INVALID_SOCKET;
        }
    }
    ClientSockets.Empty();  // 배열 비우기
}

void AMySocketActor::CloseServerSocket()
{
    if (ServerSocket != INVALID_SOCKET)
    {
        closesocket(ServerSocket);
        ServerSocket = INVALID_SOCKET;
    }
}

// Called every frame
void AMySocketActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    static float AccumulatedTime = 0.0f;
    AccumulatedTime += DeltaTime;

    if (AccumulatedTime >= 0.0166f) // 60FPS
    {
        AccumulatedTime = 0.0f;
        FScopeLock Lock(&ClientSocketsMutex);
        for (SOCKET ClientSocket : ClientSockets)
        {
            if (ClientSocket != INVALID_SOCKET)
            {
                SendCultistData(ClientSocket);
                SendPoliceData(ClientSocket);
            }
        }
        CheckImpactEffect();
    }
}