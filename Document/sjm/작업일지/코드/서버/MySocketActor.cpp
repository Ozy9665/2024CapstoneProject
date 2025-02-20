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
}

// Called when the game starts or when spawned
void AMySocketActor::BeginPlay()
{
    Super::BeginPlay();
    ServerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
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
            BlockMap.Add(BlockIndex, Block);
            BlockLocations.Add(BlockIndex, Block->GetActorLocation());
            UE_LOG(LogTemp, Error, TEXT("Added Block: ID=%d"), BlockIndex);
            BlockIndex++;
        }
    }
}

void AMySocketActor::SendData(SOCKET TargetSocket)
{
    SendPlayerData(TargetSocket);
    SendObjectData(TargetSocket);
}

void AMySocketActor::SendPlayerData(SOCKET TargetSocket)
{
    FScopeLock Lock(&ClientSocketsMutex);

    TArray<FCharacterState> AllCharacterStates;
    AllCharacterStates.Reserve(ClientStates.Num() + 1); // 메모리 할당 최적화

    // 다른 클라이언트들의 캐릭터 상태 추가
    for (auto& Entry : ClientStates)
    {
        if (Entry.Key != TargetSocket)
        {
            AllCharacterStates.Add(Entry.Value);
        }
    }

    // 서버 캐릭터 상태 추가
    if (ServerCharacter)
    {
        ServerState = GetServerCharacterState();
        AllCharacterStates.Add(ServerState);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Server character not initialized!"));
    }

    int32 TotalBytes = sizeof(uint8) + (AllCharacterStates.Num() * sizeof(FCharacterState));

    TArray<uint8> PacketData;
    PacketData.SetNumUninitialized(TotalBytes);

    memcpy(PacketData.GetData(), &playerHeader, sizeof(uint8));
    memcpy(PacketData.GetData() + sizeof(uint8), AllCharacterStates.GetData(), AllCharacterStates.Num() * sizeof(FCharacterState));

    send(TargetSocket, reinterpret_cast<const char*>(PacketData.GetData()), TotalBytes, 0);
}

void AMySocketActor::SendObjectData(SOCKET TargetSocket)
{
    FScopeLock Lock(&ClientSocketsMutex);

    if (BlockLocations.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("send object returned"));
        return; // 전송할 블록이 없으면 종료
    }

    int32 TotalBytes = sizeof(uint8) + (BlockLocations.Num() * (sizeof(int32) + sizeof(FVector)));

    TArray<uint8> PacketData;
    PacketData.SetNumUninitialized(TotalBytes);

    uint8 Header = objectHeader;
    memcpy(PacketData.GetData(), &objectHeader, sizeof(uint8));

    int32 Offset = sizeof(uint8);
    for (auto& Block : BlockLocations)
    {
        int32 BlockID = Block.Key;
        memcpy(PacketData.GetData() + Offset, &Block.Key, sizeof(int32));
        Offset += sizeof(int32);

        FVector Location = Block.Value;
        memcpy(PacketData.GetData() + Offset, &Location, sizeof(FVector));
        Offset += sizeof(FVector);
    }

    send(TargetSocket, reinterpret_cast<const char*>(PacketData.GetData()), TotalBytes, 0);
}

FCharacterState AMySocketActor::GetServerCharacterState()
{
    FCharacterState State;
    State.PlayerID = -1;

    // 위치 및 회전 설정
    State.PositionX = ServerCharacter->GetActorLocation().X;
    State.PositionY = ServerCharacter->GetActorLocation().Y;
    State.PositionZ = ServerCharacter->GetActorLocation().Z;
    State.RotationPitch = ServerCharacter->GetActorRotation().Pitch;
    State.RotationYaw = ServerCharacter->GetActorRotation().Yaw;
    State.RotationRoll = ServerCharacter->GetActorRotation().Roll;

    // 속도 및 GroundSpeed 계산
    FVector Velocity = ServerCharacter->GetVelocity();
    State.VelocityX = Velocity.X;
    State.VelocityY = Velocity.Y;
    State.VelocityZ = Velocity.Z;
    State.GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

    // IsFalling 상태
    UCharacterMovementComponent* MovementComp = ServerCharacter->GetCharacterMovement();
    State.bIsFalling = MovementComp ? MovementComp->IsFalling() : false;

    // AnimationState 계산
    float Speed = Velocity.Size();
    if (Speed < KINDA_SMALL_NUMBER)
    {
        State.AnimationState = EAnimationState::Idle; // 정지 상태
    }
    else
    {
        State.AnimationState = EAnimationState::Running; // 이동 상태
    }

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

                    if (PacketType == playerHeader)
                    {
                        ProcessPlayerData(ClientSocket, Buffer, BytesReceived);
                    }
                    else if (PacketType == objectHeader)
                    {
                        ProcessObjectData(ClientSocket, Buffer, BytesReceived);
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
    if (BytesReceived < sizeof(uint8) + sizeof(FCharacterState))
    {
        UE_LOG(LogTemp, Warning, TEXT("Received player data packet size mismatch."));
        return;
    }    

    FCharacterState ReceivedState;
    FMemory::Memcpy(&ReceivedState, Buffer + sizeof(uint8), sizeof(FCharacterState));

    ReceivedState.PlayerID = static_cast<int32>(ClientSocket);

    AsyncTask(ENamedThreads::GameThread, [this, ClientSocket, ReceivedState]()
        {
            FScopeLock Lock(&ClientSocketsMutex);
            ClientStates.FindOrAdd(ClientSocket) = ReceivedState;
            SpawnOrUpdateClientCharacter(ClientSocket, ReceivedState);
        });
}

void AMySocketActor::ProcessObjectData(SOCKET ClientSocket, char* Buffer, int32 BytesReceived) {
    int32 Offset = sizeof(uint8);

    while (Offset + sizeof(int32) + sizeof(FVector) <= BytesReceived) // 안전 체크
    {
        int32 BlockID;
        memcpy(&BlockID, Buffer + Offset, sizeof(int32));
        Offset += sizeof(int32);

        FVector BlockPos;
        memcpy(&BlockPos, Buffer + Offset, sizeof(FVector));
        Offset += sizeof(FVector);

        if (AReplicatedPhysicsBlock* Block = BlockMap.FindRef(BlockID))
        {
            AsyncTask(ENamedThreads::GameThread, [this, BlockID, Block, BlockPos]()
                {
                    Block->SetActorLocation(BlockPos);

                    // **BlockLocations 갱신**
                    {
                        FScopeLock Lock(&ClientSocketsMutex);
                        BlockLocations.FindOrAdd(BlockID) = BlockPos;
                    }

                    UE_LOG(LogTemp, Log, TEXT("Updated Block: ID=%d, New Location=(%.2f, %.2f, %.2f)"),
                        BlockID, BlockPos.X, BlockPos.Y, BlockPos.Z);
                });
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Block ID not found: %d"), BlockID);
        }
    }
}


void AMySocketActor::SpawnClientCharacter(SOCKET ClientSocket, const FCharacterState& State)
{
    // 새로운 클라이언트 캐릭터 생성
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // 각 클라이언트에 대한 고유 위치 생성 (예: 소켓 값을 기반으로 오프셋 계산)
    FVector SpawnLocation = FVector((ClientSocket % 10) * 200.0f, (ClientSocket / 10) * 200.0f, 100.0f);

    UClass* BP_ClientCharacter = LoadClass<ACharacter>(
        nullptr, TEXT("/Game/Characters/Mannequins/Meshes/BP_ClientCharacter.BP_ClientCharacter_C"));
    if (BP_ClientCharacter)
    {
        ACharacter* NewCharacter = GetWorld()->SpawnActor<ACharacter>(
            BP_ClientCharacter, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
        if (NewCharacter)
        {
            ClientCharacters.Add(ClientSocket, NewCharacter);
            UE_LOG(LogTemp, Log, TEXT("Client character spawned for socket %d at location %s"),
                ClientSocket, *SpawnLocation.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn client character for socket %d"), ClientSocket);
        }
    }
}

void AMySocketActor::SpawnOrUpdateClientCharacter(SOCKET ClientSocket, const FCharacterState& State)
{
    AsyncTask(ENamedThreads::GameThread, [this, ClientSocket, State]()
        {
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

void AMySocketActor::UpdateCharacterState(ACharacter* Character, const FCharacterState& State)
{
    const float InterpSpeed = 30.0f; // 보간 속도 
    const float DeltaTime = GetWorld()->GetDeltaSeconds();

    FVector CurrentLocation = Character->GetActorLocation();
    FVector TargetLocation = FVector(State.PositionX, State.PositionY, State.PositionZ);

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

void AMySocketActor::UpdateAnimInstanceProperties(UAnimInstance* AnimInstance, const FCharacterState& State)
{
    // ShouldMove 업데이트
    FProperty* ShouldMoveProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ShouldMove"));
    if (ShouldMoveProperty && ShouldMoveProperty->IsA<FBoolProperty>())
    {
        bool bShouldMove = (State.AnimationState == EAnimationState::Running);
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(ShouldMoveProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, bShouldMove);
    }

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

    // GroundSpeed 업데이트
    FProperty* GroundSpeedProperty = AnimInstance->GetClass()->FindPropertyByName(FName("GroundSpeed"));
    if (GroundSpeedProperty && GroundSpeedProperty->IsA<FDoubleProperty>())
    {
        double GroundSpeed = static_cast<double>(FVector(State.VelocityX, State.VelocityY, 0.0f).Size());
        FDoubleProperty* DoubleProp = CastFieldChecked<FDoubleProperty>(GroundSpeedProperty);
        DoubleProp->SetPropertyValue_InContainer(AnimInstance, GroundSpeed);
    }

    // IsFalling 업데이트
    FProperty* IsFallingProperty = AnimInstance->GetClass()->FindPropertyByName(FName("IsFalling"));
    if (IsFallingProperty && IsFallingProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsFallingProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, State.bIsFalling);
    }
}

void AMySocketActor::CloseClientSocket(SOCKET ClientSocket)
{
    AsyncTask(ENamedThreads::GameThread, [this, ClientSocket]()
        {
            FScopeLock Lock(&ClientSocketsMutex);
            if (ClientCharacters.Contains(ClientSocket))
            {
                ACharacter* Character = ClientCharacters[ClientSocket];
                if (Character)
                {
                    Character->Destroy();
                }
                ClientCharacters.Remove(ClientSocket);
            }
            ClientSockets.Remove(ClientSocket);
            closesocket(ClientSocket);
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
                SendData(ClientSocket);
            }
        }
    }
}