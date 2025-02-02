// Fill out your copyright notice in the Description page of Project Settings.


#include "MySocketActor.h"
#include "Engine/Engine.h"
#include <GameFramework/Character.h>
#include "GameFramework/CharacterMovementComponent.h"
#include <Kismet/GameplayStatics.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")  // Winsock ���̺귯�� ��ũ

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
    /*if (InitializeServer(7777))
    {
        UE_LOG(LogTemp, Log, TEXT("Server initialized and waiting for clients..."));
        AcceptClientAsync();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to initialize server."));
    }*/
    if (InitializeUDPSocket(8888)) // UDP ��Ʈ ����
    {
        UE_LOG(LogTemp, Log, TEXT("UDP Broadcast initialized successfully."));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to initialize UDP broadcast socket."));
    }
}

void AMySocketActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    bIsRunning = false;

    // ��� Ŭ���̾�Ʈ ���� �ݱ�
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
        ClientSockets.Empty();  // �迭 ����
    }

    // ���� ���� �ݱ�
    if (ServerSocket != INVALID_SOCKET)
    {
        closesocket(ServerSocket);
        ServerSocket = INVALID_SOCKET;
    }

    // UDP ��ε�ĳ��Ʈ ���� �ݱ�
    if (BroadcastSocket != INVALID_SOCKET)
    {
        closesocket(BroadcastSocket);
        BroadcastSocket = INVALID_SOCKET;
    }

    // Winsock ����
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
        UE_LOG(LogTemp, Error, TEXT("Failed to create socket: %ld"), WSAGetLastError());
        WSACleanup();
        return false;
    }

    sockaddr_in ServerAddr;
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port = htons(Port);

    if (bind(ServerSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)) == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("Bind failed with error: %ld"), WSAGetLastError());
        closesocket(ServerSocket);
        WSACleanup();
        return false;
    }

    if (listen(ServerSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("Listen failed with error: %ld"), WSAGetLastError());
        closesocket(ServerSocket);
        WSACleanup();
        return false;
    }

    return true;
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

                // Ŭ���̾�Ʈ���� �����͸� �񵿱� ����
                ReceiveData(NewClientSocket);
            }
        });
}

void AMySocketActor::sendData(SOCKET TargetSocket)
{
    FScopeLock Lock(&ClientSocketsMutex);

    TArray<FCharacterState> AllCharacterStates;
    AllCharacterStates.Reserve(ClientStates.Num() + 1); // �޸� �Ҵ� ����ȭ

    // �ٸ� Ŭ���̾�Ʈ���� ĳ���� ���� �߰�
    for (auto& Entry : ClientStates)
    {
        if (Entry.Key != TargetSocket)
        {
            AllCharacterStates.Add(Entry.Value);

            /*UE_LOG(LogTemp, Log, TEXT("Forwarding data to socket %d: PlayerID=%d, Position(%.2f, %.2f, %.2f), Velocity(%.2f, %.2f, %.2f), AnimationState=%d"),
                TargetSocket, State.PlayerID, State.PositionX, State.PositionY, State.PositionZ,
                State.VelocityX, State.VelocityY, State.VelocityZ, static_cast<int32>(State.AnimationState));*/
        }
    }

    // ���� ĳ���� ���� �߰�
    if (ServerCharacter)
    {
        ServerState.PlayerID = -1;

        // ��ġ �� ȸ�� ����
        ServerState.PositionX = ServerCharacter->GetActorLocation().X;
        ServerState.PositionY = ServerCharacter->GetActorLocation().Y;
        ServerState.PositionZ = ServerCharacter->GetActorLocation().Z;
        ServerState.RotationPitch = ServerCharacter->GetActorRotation().Pitch;
        ServerState.RotationYaw = ServerCharacter->GetActorRotation().Yaw;
        ServerState.RotationRoll = ServerCharacter->GetActorRotation().Roll;

        // �ӵ� �� GroundSpeed ���
        FVector Velocity = ServerCharacter->GetVelocity();
        ServerState.VelocityX = Velocity.X;
        ServerState.VelocityY = Velocity.Y;
        ServerState.VelocityZ = Velocity.Z;
        ServerState.GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

        // IsFalling ����
        UCharacterMovementComponent* MovementComp = ServerCharacter->GetCharacterMovement();
        ServerState.bIsFalling = MovementComp ? MovementComp->IsFalling() : false;

        // AnimationState ���
        float Speed = Velocity.Size();
        if (Speed < KINDA_SMALL_NUMBER)
        {
            ServerState.AnimationState = EAnimationState::Idle; // ���� ����
        }
        else
        {
            ServerState.AnimationState = EAnimationState::Running; // �̵� ����
        }

        AllCharacterStates.Add(ServerState);

        /*UE_LOG(LogTemp, Log, TEXT("Server Character: Position(%.2f, %.2f, %.2f), Velocity(%.2f, %.2f, %.2f), AnimationState=%d"),
            ServerState.PositionX, ServerState.PositionY, ServerState.PositionZ,
            ServerState.VelocityX, ServerState.VelocityY, ServerState.VelocityZ, static_cast<int32>(ServerState.AnimationState));*/
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Server character not initialized!"));
    }

    int32 TotalBytes = AllCharacterStates.Num() * sizeof(FCharacterState);
    send(TargetSocket, reinterpret_cast<const char*>(AllCharacterStates.GetData()), TotalBytes, 0);

    //UE_LOG(LogTemp, Log, TEXT("Sent %d character states to socket %d"), AllCharacterStates.Num(), TargetSocket);
}

void AMySocketActor::ReceiveData(SOCKET ClientSocket)
{
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, ClientSocket]()
        {
            while (true)
            {
                char Buffer[sizeof(FCharacterState)];
                int32 BytesReceived = recv(ClientSocket, Buffer, sizeof(Buffer), 0);

                if (BytesReceived > 0)
                {
                    // UE_LOG(LogTemp, Log, TEXT("Data received from socket %d: %d bytes"), ClientSocket, BytesReceived);

                    if (BytesReceived == sizeof(FCharacterState))
                    {
                        FCharacterState ReceivedState;
                        FMemory::Memcpy(&ReceivedState, Buffer, sizeof(FCharacterState));
                        ReceivedState.PlayerID = static_cast<int32>(ClientSocket);

                        {
                            // �����͸� ClientStates �ʿ� ����
                            FScopeLock Lock(&ClientSocketsMutex);
                            ClientStates.FindOrAdd(ClientSocket) = ReceivedState;

                            UE_LOG(LogTemp, Log, TEXT("Updated ClientStates for socket %d: PlayerID=%d, Position(%.2f, %.2f, %.2f), Velocity(%.2f, %.2f, %.2f), AnimationState=%d"),
                                ClientSocket,
                                ReceivedState.PlayerID,
                                ReceivedState.PositionX, ReceivedState.PositionY, ReceivedState.PositionZ,
                                ReceivedState.VelocityX, ReceivedState.VelocityY, ReceivedState.VelocityZ,
                                static_cast<int32>(ReceivedState.AnimationState));
                        }

                        // Ŭ���̾�Ʈ ���� ������� ó��
                        SpawnOrUpdateClientCharacter(ClientSocket, ReceivedState);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Unexpected data size from socket %d: %d"), ClientSocket, BytesReceived);
                    }
                }
                else if (BytesReceived == 0 || WSAGetLastError() == WSAECONNRESET)
                {
                    UE_LOG(LogTemp, Log, TEXT("Client disconnected. Socket: %d"), ClientSocket);

                    {
                        FScopeLock Lock(&ClientSocketsMutex);
                        ClientStates.Remove(ClientSocket);
                    }

                    CloseClientSocket(ClientSocket);
                    break;
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("recv failed on socket %d with error: %ld"), ClientSocket, WSAGetLastError());

                    {
                        FScopeLock Lock(&ClientSocketsMutex);
                        ClientStates.Remove(ClientSocket);
                    }

                    CloseClientSocket(ClientSocket);
                    break;
                }
            }
        });
}

void AMySocketActor::SpawnClientCharacter(SOCKET ClientSocket, const FCharacterState& State)
{
    // ���ο� Ŭ���̾�Ʈ ĳ���� ����
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // �� Ŭ���̾�Ʈ�� ���� ���� ��ġ ���� (��: ���� ���� ������� ������ ���)
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
                // ���� ĳ���� ������Ʈ
                ACharacter* Character = ClientCharacters[ClientSocket];
                if (Character)
                {
                    const float InterpSpeed = 30.0f; // ���� �ӵ� 
                    const float DeltaTime = GetWorld()->GetDeltaSeconds();

                    FVector CurrentLocation = Character->GetActorLocation();
                    FVector TargetLocation(State.PositionX, State.PositionY, State.PositionZ);

                    FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, InterpSpeed);
                    Character->SetActorLocation(NewLocation);

                    Character->SetActorRotation(FRotator(State.RotationPitch, State.RotationYaw, State.RotationRoll));

                    // �ִϸ��̼� ���� ������Ʈ �߰�
                    if (USkeletalMeshComponent* Mesh = Character->GetMesh())
                    {
                        UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
                        if (AnimInstance)
                        {
                            // ShouldMove ������Ʈ
                            FProperty* ShouldMoveProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ShouldMove"));
                            if (ShouldMoveProperty && ShouldMoveProperty->IsA<FBoolProperty>())
                            {
                                bool bShouldMove = (State.AnimationState == EAnimationState::Running);
                                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(ShouldMoveProperty);
                                BoolProp->SetPropertyValue_InContainer(AnimInstance, bShouldMove);
                            }

                            // Velocity ������Ʈ
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

                            // GroundSpeed ������Ʈ
                            FProperty* GroundSpeedProperty = AnimInstance->GetClass()->FindPropertyByName(FName("GroundSpeed"));
                            if (GroundSpeedProperty && GroundSpeedProperty->IsA<FDoubleProperty>())
                            {
                                double GroundSpeed = static_cast<double>(FVector(State.VelocityX, State.VelocityY, 0.0f).Size());
                                FDoubleProperty* DoubleProp = CastFieldChecked<FDoubleProperty>(GroundSpeedProperty);
                                DoubleProp->SetPropertyValue_InContainer(AnimInstance, GroundSpeed);
                            }

                            // IsFalling ������Ʈ
                            FProperty* IsFallingProperty = AnimInstance->GetClass()->FindPropertyByName(FName("IsFalling"));
                            if (IsFallingProperty && IsFallingProperty->IsA<FBoolProperty>())
                            {
                                FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsFallingProperty);
                                BoolProp->SetPropertyValue_InContainer(AnimInstance, State.bIsFalling);
                            }
                        }
                    }
                }
            }
            else {
                SpawnClientCharacter(ClientSocket, State);
            }
        });
}

void AMySocketActor::CloseClientSocket(SOCKET ClientSocket)
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
        BroadcastData();
        /*FScopeLock Lock(&ClientSocketsMutex);
        for (SOCKET ClientSocket : ClientSockets)
        {
            if (ClientSocket != INVALID_SOCKET)
            {
                sendData(ClientSocket);
            }
        }*/
    }
}

bool AMySocketActor::InitializeUDPSocket(int32 Port)
{
    WSADATA WsaData;
    if (WSAStartup(MAKEWORD(2, 2), &WsaData) != 0)
    {
        UE_LOG(LogTemp, Error, TEXT("WSAStartup failed"));
        return false;
    }

    // UDP ���� ����
    BroadcastSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (BroadcastSocket == INVALID_SOCKET)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create UDP socket: %ld"), WSAGetLastError());
        WSACleanup();
        return false;
    }

    // ��ε�ĳ��Ʈ Ȱ��ȭ �ɼ� ����
    BOOL bBroadcast = TRUE;
    if (setsockopt(BroadcastSocket, SOL_SOCKET, SO_BROADCAST, (char*)&bBroadcast, sizeof(bBroadcast)) == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to set socket broadcast option: %ld"), WSAGetLastError());
        closesocket(BroadcastSocket);
        WSACleanup();
        return false;
    }

    sockaddr_in ServerAddr;
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port = htons(Port);

    // ���� ���ε�
    if (bind(BroadcastSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)) == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("Bind failed with error: %ld"), WSAGetLastError());
        closesocket(BroadcastSocket);
        WSACleanup();
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("UDP Broadcast Socket Initialized on Port %d"), Port);
    return true;
}

void AMySocketActor::BroadcastData()
{
    if (BroadcastSocket == INVALID_SOCKET)
    {
        UE_LOG(LogTemp, Error, TEXT("Broadcast socket is not initialized!"));
        return;
    }

    TArray<FCharacterState> AllCharacterStates;
    AllCharacterStates.Reserve(ClientStates.Num() + 1);

    for (auto& Entry : ClientStates)
    {
        AllCharacterStates.Add(Entry.Value);
    }

    if (ServerCharacter)
    {
        ServerState.PlayerID = -1;
        FVector Location = ServerCharacter->GetActorLocation();
        FVector Velocity = ServerCharacter->GetVelocity();

        ServerState.PositionX = Location.X;
        ServerState.PositionY = Location.Y;
        ServerState.PositionZ = Location.Z;

        ServerState.VelocityX = Velocity.X;
        ServerState.VelocityY = Velocity.Y;
        ServerState.VelocityZ = Velocity.Z;

        UCharacterMovementComponent* MovementComp = ServerCharacter->GetCharacterMovement();
        ServerState.bIsFalling = MovementComp ? MovementComp->IsFalling() : false;

        float Speed = Velocity.Size();
        ServerState.AnimationState = (Speed < KINDA_SMALL_NUMBER) ? EAnimationState::Idle : EAnimationState::Running;

        AllCharacterStates.Add(ServerState);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Server character not initialized!"));
    }

    int32 TotalBytes = AllCharacterStates.Num() * sizeof(FCharacterState);
    const char* SendBuffer = reinterpret_cast<const char*>(AllCharacterStates.GetData());

    sockaddr_in BroadcastAddr;
    BroadcastAddr.sin_family = AF_INET;
    BroadcastAddr.sin_port = htons(7777); // ��ε�ĳ��Ʈ ��Ʈ
    BroadcastAddr.sin_addr.s_addr = INADDR_BROADCAST; // ��Ʈ��ũ ��ε�ĳ��Ʈ

    int SentBytes = sendto(BroadcastSocket, SendBuffer, TotalBytes, 0,
        (SOCKADDR*)&BroadcastAddr, sizeof(BroadcastAddr));

    if (SentBytes == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("Broadcast send failed: %ld"), WSAGetLastError());
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Broadcasted %d bytes to all clients"), SentBytes);
    }
}
