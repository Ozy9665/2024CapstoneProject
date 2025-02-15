// Fill out your copyright notice in the Description page of Project Settings.

#include "MySocketClientActor.h"
#include "Engine/Engine.h"
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Kismet/GameplayStatics.h>
#include <GameFramework/Character.h>
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

#pragma comment(lib, "ws2_32.lib")

// Sets default values
AMySocketClientActor::AMySocketClientActor()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    ClientSocket = INVALID_SOCKET;
}

// Called when the game starts or when spawned
void AMySocketClientActor::BeginPlay()
{
    Super::BeginPlay();

    FString ServerIP = TEXT("127.0.0.1");  // 서버 IP
    int32 ServerPort = 7777;              // 서버 포트

    if (ConnectToServer(ServerIP, ServerPort))
    {
        UE_LOG(LogTemp, Log, TEXT("Connected to server!"));
        ReceiveData();  // 데이터 수신 시작
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to connect to server."));
    }
}

void AMySocketClientActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (ClientSocket != INVALID_SOCKET)
    {
        closesocket(ClientSocket);
        ClientSocket = INVALID_SOCKET;
    }

    WSACleanup();
    UE_LOG(LogTemp, Log, TEXT("Client socket closed and cleaned up."));
}

bool AMySocketClientActor::ConnectToServer(const FString& ServerIP, int32 ServerPort)
{
    WSADATA WsaData;
    int Result = WSAStartup(MAKEWORD(2, 2), &WsaData);
    if (Result != 0)
    {
        UE_LOG(LogTemp, Error, TEXT("WSAStartup failed: %d"), Result);
        return false;
    }

    ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ClientSocket == INVALID_SOCKET)
    {
        LogAndCleanupSocketError(TEXT("Failed to create socket"));
        return false;
    }

    sockaddr_in ServerAddr;
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(ServerPort);
    ServerAddr.sin_addr.s_addr = inet_addr(TCHAR_TO_ANSI(*ServerIP));

    if (connect(ClientSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)) == SOCKET_ERROR)
    {
        LogAndCleanupSocketError(TEXT("Connection failed"));
        return false;
    }

    return true;
}

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
            // UE_LOG(LogTemp, Log, TEXT("Block: ID=%d"), BlockIndex);
            BlockIndex++;
        }
    }
}

void AMySocketClientActor::LogAndCleanupSocketError(const TCHAR* ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("%s with error: %ld"), ErrorMessage, WSAGetLastError());
    if (ClientSocket != INVALID_SOCKET)
    {
        closesocket(ClientSocket);
        ClientSocket = INVALID_SOCKET;
    }
    WSACleanup();
}

void AMySocketClientActor::ReceiveData()
{
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
        {
            while (true)
            {
                const int32 BufferSize = 1024 * sizeof(FCharacterState);
                char Buffer[BufferSize];

                int32 BytesReceived = recv(ClientSocket, Buffer, BufferSize, 0);

                if (BytesReceived > 0)
                {
                    ProcessReceivedData(Buffer, BytesReceived);
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

void AMySocketClientActor::ProcessReceivedData(char* Buffer, int32 BytesReceived)
{
    int32 Offset = 0;
    static bool bInitializedBlocks = false;

    while (Offset < BytesReceived)
    {
        uint8 PacketType;
        memcpy(&PacketType, Buffer + Offset, sizeof(uint8));
        Offset += sizeof(uint8);

        if (PacketType == playerHeader) // 플레이어 데이터
        {
            int32 NumPlayers = (BytesReceived - Offset) / sizeof(FCharacterState);
            FCharacterState* ReceivedStates = reinterpret_cast<FCharacterState*>(Buffer + Offset);

            for (int32 i = 0; i < NumPlayers; i++)
            {
                FCharacterState& State = ReceivedStates[i];
                FString PlayerID = FString::Printf(TEXT("Player_%d"), State.PlayerID);

                {
                    FScopeLock Lock(&ReceivedDataMutex);
                    ReceivedCharacterStates.Add(PlayerID, State);
                }
            }
            Offset += NumPlayers * sizeof(FCharacterState);
        }
        else if (PacketType == objectHeader) // 블록 데이터
        {
            if (!bInitializedBlocks)
            {
                AsyncTask(ENamedThreads::GameThread, [this]()
                    {
                        InitializeBlocks();
                    });
                bInitializedBlocks = true;
            }
            while (Offset < BytesReceived)
            {
                int32 BlockID;
                memcpy(&BlockID, Buffer + Offset, sizeof(int32));
                Offset += sizeof(int32);

                FVector BlockPos;
                memcpy(&BlockPos, Buffer + Offset, sizeof(FVector));
                Offset += sizeof(FVector);

                if (AReplicatedPhysicsBlock* Block = SyncedBlocks.FindRef(BlockID))
                {
                    AsyncTask(ENamedThreads::GameThread, [Block, BlockPos]()
                        {
                            Block->SetActorLocation(BlockPos);
                        });
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Unknown packet type received: %d"), PacketType);
            break;
        }
    }
}

void AMySocketClientActor::SpawnCharacter(const FCharacterState& State)
{
    // PlayerID를 기반으로 고유 키 생성
    FString CharacterKey = FString::Printf(TEXT("Character_%d"), State.PlayerID);

    // 이미 캐릭터가 존재하면 아무 작업도 하지 않음
    if (SpawnedCharacters.Contains(CharacterKey))
    {
        return;
    }
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    UClass* BP_ClientCharacter = LoadClass<ACharacter>(
        nullptr, TEXT("/Game/Characters/Mannequins/Meshes/BP_ClientCharacter.BP_ClientCharacter_C"));

    if (BP_ClientCharacter)
    {
        ACharacter* NewCharacter = GetWorld()->SpawnActor<ACharacter>(
            BP_ClientCharacter,
            FVector(State.PositionX, State.PositionY, State.PositionZ),
            FRotator(State.RotationPitch, State.RotationYaw, State.RotationRoll),
            SpawnParams
        );
        if (NewCharacter)
        {
            SpawnedCharacters.Add(CharacterKey, NewCharacter);
            UE_LOG(LogTemp, Log, TEXT("Spawned new character for PlayerID=%d at Position=(%f, %f, %f)"),
                State.PlayerID, State.PositionX, State.PositionY, State.PositionZ);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn character for PlayerID=%d"), State.PlayerID);
        }
    }
}

void AMySocketClientActor::SendData() {
    SendPlayerData();
    SendObjectData();
}

void AMySocketClientActor::SendPlayerData()
{
    // 데이터 보내기
    if (ClientSocket != INVALID_SOCKET)
    {
        // 캐릭터 상태 가져오기
        ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
        if (PlayerCharacter)
        {
            FCharacterState CharacterState = GetCharacterState(PlayerCharacter);

            TArray<uint8> PacketData;
            PacketData.SetNumUninitialized(sizeof(uint8) + sizeof(FCharacterState));

            uint8 Header = playerHeader;
            memcpy(PacketData.GetData(), &Header, sizeof(uint8));
            memcpy(PacketData.GetData() + sizeof(uint8), &CharacterState, sizeof(FCharacterState));

            int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(PacketData.GetData()), PacketData.Num(), 0);
            if (BytesSent == SOCKET_ERROR)
            {
                UE_LOG(LogTemp, Error, TEXT("SendPlayerData failed with error: %ld"), WSAGetLastError());
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("PlayerCharacter is null."));
        }
    }
}

void AMySocketClientActor::SendObjectData()
{
    if (ClientSocket == INVALID_SOCKET || SyncedBlocks.Num() == 0)
    {
        return;
    }

    TArray<uint8> PacketData;
    int32 TotalSize = sizeof(uint8) + (SyncedBlocks.Num() * (sizeof(int32) + sizeof(FVector)));
    PacketData.SetNumUninitialized(TotalSize);

    uint8 Header = objectHeader;
    memcpy(PacketData.GetData(), &Header, sizeof(uint8));

    int32 Offset = sizeof(uint8);
    for (auto& Block : SyncedBlocks)
    {
        int32 BlockID = Block.Key;
        FVector BlockPos = Block.Value->GetActorLocation();

        memcpy(PacketData.GetData() + Offset, &BlockID, sizeof(int32));
        Offset += sizeof(int32);

        memcpy(PacketData.GetData() + Offset, &BlockPos, sizeof(FVector));
        Offset += sizeof(FVector);
    }

    int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(PacketData.GetData()), PacketData.Num(), 0);
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendBlockData failed with error: %ld"), WSAGetLastError());
    }
}

FCharacterState AMySocketClientActor::GetCharacterState(ACharacter* PlayerCharacter)
{
    FCharacterState CharacterState;
    CharacterState.PositionX = PlayerCharacter->GetActorLocation().X;
    CharacterState.PositionY = PlayerCharacter->GetActorLocation().Y;
    CharacterState.PositionZ = PlayerCharacter->GetActorLocation().Z;

    CharacterState.RotationPitch = PlayerCharacter->GetActorRotation().Pitch;
    CharacterState.RotationYaw = PlayerCharacter->GetActorRotation().Yaw;
    CharacterState.RotationRoll = PlayerCharacter->GetActorRotation().Roll;

    FVector Velocity = PlayerCharacter->GetVelocity();
    CharacterState.VelocityX = Velocity.X;
    CharacterState.VelocityY = Velocity.Y;
    CharacterState.VelocityZ = Velocity.Z;
    CharacterState.GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

    UCharacterMovementComponent* MovementComp = PlayerCharacter->GetCharacterMovement();
    CharacterState.bIsFalling = MovementComp ? MovementComp->IsFalling() : false;

    float Speed = Velocity.Size();
    if (Speed < KINDA_SMALL_NUMBER)
    {
        CharacterState.AnimationState = EAnimationState::Idle; // 속도가 거의 0이면 Idle
    }
    else
    {
        CharacterState.AnimationState = EAnimationState::Running; // 이동 중이면 Run
    }

    return CharacterState;
}

void AMySocketClientActor::ProcessCharacterUpdates(float DeltaTime)
{    
    FScopeLock Lock(&ReceivedDataMutex);

    for (auto& Pair : ReceivedCharacterStates)
    {
        const FString& CharacterKey = Pair.Key;
        const FCharacterState& State = Pair.Value;

        if (ACharacter* Character = SpawnedCharacters.FindRef(CharacterKey))
        {
            UpdateCharacterState(Character, State, DeltaTime);
        }
        else
        {
            SpawnCharacter(State);
        }
    }
}

void AMySocketClientActor::UpdateCharacterState(ACharacter* Character, const FCharacterState& State, float DeltaTime)
{
    // 이전 위치 저장
    FVector CurrentLocation = Character->GetActorLocation();
    FVector TargetLocation(State.PositionX, State.PositionY, State.PositionZ);

    // 예측 이동
    static TMap<FString, FVector> LastVelocity;
    FVector& PreviousVelocity = LastVelocity.FindOrAdd(Character->GetName(), FVector::ZeroVector);

    FVector EstimatedLocation = CurrentLocation + (PreviousVelocity * DeltaTime);
    FVector NewTarget = FMath::Lerp(EstimatedLocation, TargetLocation, 0.75f); // 75% 실제 위치 반영

    // 보간
    float InterpSpeed = 30.0f; // 보간 속도
    FVector InterpolatedLocation = FMath::VInterpTo(CurrentLocation, NewTarget, DeltaTime, InterpSpeed);

    Character->SetActorLocation(InterpolatedLocation);
    Character->SetActorRotation(FRotator(State.RotationPitch, State.RotationYaw, State.RotationRoll));

    // 속도 업데이트
    PreviousVelocity = FVector(State.VelocityX, State.VelocityY, State.VelocityZ);

    // 애니메이션 상태 업데이트
    if (USkeletalMeshComponent* Mesh = Character->GetMesh())
    {
        UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
        if (AnimInstance)
        {
            UpdateAnimInstanceProperties(AnimInstance, State);
        }
    }
}

void AMySocketClientActor::UpdateAnimInstanceProperties(UAnimInstance* AnimInstance, const FCharacterState& State)
{
    FProperty* ShouldMoveProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ShouldMove"));
    if (ShouldMoveProperty && ShouldMoveProperty->IsA<FBoolProperty>())
    {
        bool bShouldMove = FVector(State.VelocityX, State.VelocityY, 0.0f).Size() > 10.0f;
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
}

// Called every frame
void AMySocketClientActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    SendData();
    ProcessCharacterUpdates(DeltaTime);
}