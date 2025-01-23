// Fill out your copyright notice in the Description page of Project Settings.


#include "MySocketActor.h"
#include "Engine/Engine.h"
#include <GameFramework/Character.h>
#include "GameFramework/CharacterMovementComponent.h"
#include <Kismet/GameplayStatics.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "ClientCharacter.h"

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
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
        {
            while (true)
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

    TArray<FCharacterState> OtherCharacters;

    // �ٸ� Ŭ���̾�Ʈ���� ĳ���� ���� �߰�
    for (auto& Entry : ClientCharacters)
    {
        SOCKET CurrentSocket = Entry.Key;
        AClientCharacter* Character = Entry.Value;

        if (CurrentSocket != TargetSocket && Character)
        {
            FCharacterState State;
            State.PlayerID = static_cast<int32>(CurrentSocket);

            // ��ġ �� ȸ�� ����
            State.PositionX = Character->GetActorLocation().X;
            State.PositionY = Character->GetActorLocation().Y;
            State.PositionZ = Character->GetActorLocation().Z;
            State.RotationPitch = Character->GetActorRotation().Pitch;
            State.RotationYaw = Character->GetActorRotation().Yaw;
            State.RotationRoll = Character->GetActorRotation().Roll;

            // �ӵ� �� GroundSpeed ���
            FVector Velocity = Character->GetVelocity();
            State.VelocityX = Velocity.X;
            State.VelocityY = Velocity.Y;
            State.VelocityZ = Velocity.Z;
            State.GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

            // IsFalling ����
            UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
            State.bIsFalling = MovementComp ? MovementComp->IsFalling() : false;

            // AnimationState ���
            float Speed = Velocity.Size();
            if (Speed < KINDA_SMALL_NUMBER)
            {
                State.AnimationState = EAnimationState::Idle; // ���� ����
            }
            else
            {
                State.AnimationState = EAnimationState::Running; // �̵� ����
            }

            // ����� ���
            UE_LOG(LogTemp, Log, TEXT("Sending data to socket %d: Position(%.2f, %.2f, %.2f), Velocity(%.2f, %.2f, %.2f), AnimationState=%d"),
                TargetSocket, State.PositionX, State.PositionY, State.PositionZ,
                State.VelocityX, State.VelocityY, State.VelocityZ, static_cast<int32>(State.AnimationState));

            OtherCharacters.Add(State);
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

        UE_LOG(LogTemp, Log, TEXT("Server Character: Position(%.2f, %.2f, %.2f), Velocity(%.2f, %.2f, %.2f), AnimationState=%d"),
            ServerState.PositionX, ServerState.PositionY, ServerState.PositionZ,
            ServerState.VelocityX, ServerState.VelocityY, ServerState.VelocityZ, static_cast<int32>(ServerState.AnimationState));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Server character not initialized!"));
    }


    UE_LOG(LogTemp, Log, TEXT("Sending server data to socket %d: Position(%.2f, %.2f, %.2f), Rotation(%.2f, %.2f, %.2f)"),
        TargetSocket, ServerState.PositionX, ServerState.PositionY, ServerState.PositionZ,
        ServerState.RotationPitch, ServerState.RotationYaw, ServerState.RotationRoll);
    UE_LOG(LogTemp, Log, TEXT("AnimationState: %d (Idle=0, Run=1)"), static_cast<int32>(ServerState.AnimationState));

    // �����͸� ����ȭ�Ͽ� ����
    for (const FCharacterState& State : OtherCharacters)
    {
        send(TargetSocket, reinterpret_cast<const char*>(&State), sizeof(FCharacterState), 0);
    }

    // ���� ���� ����
    send(TargetSocket, reinterpret_cast<const char*>(&ServerState), sizeof(FCharacterState), 0);
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
                    UE_LOG(LogTemp, Log, TEXT("Data received from socket %d: %d bytes"), ClientSocket, BytesReceived);

                    if (BytesReceived == sizeof(FCharacterState))
                    {
                        FCharacterState* ReceivedState = reinterpret_cast<FCharacterState*>(Buffer);

                        // Ŭ���̾�Ʈ ���� ������� ó��
                        SpawnOrUpdateClientCharacter(ClientSocket, *ReceivedState);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Unexpected data size from socket %d: %d"), ClientSocket, BytesReceived);
                    }
                }
                else if (BytesReceived == 0)
                {
                    UE_LOG(LogTemp, Log, TEXT("Client disconnected. Socket: %d"), ClientSocket);
                    CloseClientSocket(ClientSocket);
                    break;
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("recv failed on socket %d with error: %ld"), ClientSocket, WSAGetLastError());
                    CloseClientSocket(ClientSocket);
                    break;
                }
            }
        });
}

void AMySocketActor::SpawnOrUpdateClientCharacter(SOCKET ClientSocket, const FCharacterState& State)
{
    AsyncTask(ENamedThreads::GameThread, [this, ClientSocket, State]()
        {
            if (!ClientCharacters.Contains(ClientSocket))
            {
                // ���ο� Ŭ���̾�Ʈ ĳ���� ����
                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = this;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

                // �� Ŭ���̾�Ʈ�� ���� ���� ��ġ ���� (��: ���� ���� ������� ������ ���)
                FVector SpawnLocation = FVector((ClientSocket % 10) * 200.0f, (ClientSocket / 10) * 200.0f, 100.0f);

                UClass* BP_ClientCharacter = LoadClass<AClientCharacter>(
                    nullptr, TEXT("/Game/Characters/Mannequins/Meshes/BP_ClientCharacter.BP_ClientCharacter_C"));
                if (BP_ClientCharacter)
                {
                    AClientCharacter* NewCharacter = GetWorld()->SpawnActor<AClientCharacter>(
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
            else if (ClientCharacters.Contains(ClientSocket))
            {
                // ���� ĳ���� ������Ʈ
                AClientCharacter* Character = ClientCharacters[ClientSocket];
                if (Character)
                {
                    Character->SetActorLocation(FVector(State.PositionX, State.PositionY, State.PositionZ));
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
        });
}

void AMySocketActor::CloseClientSocket(SOCKET ClientSocket)
{
    FScopeLock Lock(&ClientSocketsMutex);
    ClientSockets.Remove(ClientSocket);
    closesocket(ClientSocket);
}

// Called every frame
void AMySocketActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    FScopeLock Lock(&ClientSocketsMutex);
    for (SOCKET ClientSocket : ClientSockets)
    {
        UE_LOG(LogTemp, Log, TEXT("Preparing to send data to socket %d"), ClientSocket);
        sendData(ClientSocket);
    }
}

