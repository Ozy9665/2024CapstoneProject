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
        InitializeBlocks();
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
            FTransform BlockTransform = Block->GetActorTransform();

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
                const int32 BufferSize = 1024;
                char Buffer[BufferSize];

                int32 BytesReceived = recv(ClientSocket, Buffer, BufferSize, 0);

                if (BytesReceived > 0)
                {
                    // 패킷의 첫 바이트는 헤더, 이후는 데이터
                    uint8 PacketType;
                    memcpy(&PacketType, Buffer, sizeof(uint8));

                    if (PacketType == playerHeader)
                    {
                        ProcessPlayerData(Buffer, BytesReceived);
                    }
                    else if (PacketType == objectHeader)
                    {
                        ProcessObjectData(Buffer, BytesReceived);
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

void AMySocketClientActor::ProcessPlayerData(char* Buffer, int32 BytesReceived)
{
    int32 Offset = sizeof(uint8); // 첫 바이트(헤더) 이후 데이터 처리

    while (Offset + sizeof(FCharacterState) <= BytesReceived) // 안전 체크
    {
        FCharacterState ReceivedState;
        memcpy(&ReceivedState, Buffer + Offset, sizeof(FCharacterState));

        FString PlayerID = FString::Printf(TEXT("Character_%d"), ReceivedState.PlayerID);

        {
            FScopeLock Lock(&ReceivedDataMutex);
            ReceivedCharacterStates.Add(PlayerID, ReceivedState);
        }

        Offset += sizeof(FCharacterState);
    }
}

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
    CharacterState.Speed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();
    
    // Crouch 상태
    CharacterState.bIsCrouching = PlayerCharacter->bIsCrouched;

    // AnimationState 계산
    if (PlayerCharacter->bIsCrouched)
    {
        CharacterState.AnimationState = EAnimationState::Crouch;
    }
    else if (CharacterState.Speed < KINDA_SMALL_NUMBER)
    {
        CharacterState.AnimationState = EAnimationState::Idle;
    }
    else
    {
        CharacterState.AnimationState = EAnimationState::Walk;
    }

    return CharacterState;
}

void AMySocketClientActor::ProcessCharacterUpdates(float DeltaTime)
{
    FScopeLock Lock(&ReceivedDataMutex);

    for (auto& Pair : ReceivedCharacterStates)
    {
        const FString& CharacterKey = FString::Printf(TEXT("Character_%d"), Pair.Value.PlayerID);
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
            double ComputedSpeed = FVector(State.VelocityX, State.VelocityY, 0.0f).Size();
            FDoubleProperty* DoubleProp = CastFieldChecked<FDoubleProperty>(SpeedProperty);
            DoubleProp->SetPropertyValue_InContainer(AnimInstance, ComputedSpeed);
        }
    }

    // IsCrouching 업데이트
    FProperty* IsCrouchingProperty = AnimInstance->GetClass()->FindPropertyByName(FName("Crouch"));
    if (IsCrouchingProperty && IsCrouchingProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsCrouchingProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, State.bIsCrouching);
    }
}

void AMySocketClientActor::SpawnCharacter(const FCharacterState& State)
{
    // PlayerID를 기반으로 고유 키 생성
    FString CharacterKey = FString::Printf(TEXT("Character_%d"), State.PlayerID);

    // 이미 캐릭터가 존재하면 아무 작업도 하지 않음
    if (SpawnedCharacters.Contains(CharacterKey))
    {
        UE_LOG(LogTemp, Warning, TEXT("Character already exists: %s"), *CharacterKey);
        return;
    }
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    UClass* BP_ClientCharacter = LoadClass<ACharacter>(
        nullptr, TEXT("/Game/Cult_Custom/Characters/BP_Cultist_A_Client.BP_Cultist_A_Client_C"));

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

// Called every frame
void AMySocketClientActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    SendPlayerData();
    ProcessCharacterUpdates(DeltaTime);
    ProcessObjectUpdates(DeltaTime);
}