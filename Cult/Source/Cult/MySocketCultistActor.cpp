// Fill out your copyright notice in the Description page of Project Settings.


#include "MySocketCultistActor.h"



// Sets default values
AMySocketCultistActor::AMySocketCultistActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

   // 파티클 코드
   /* ConstructorHelpers::FObjectFinder<UParticleSystem> ParticleAsset(TEXT("ParticleSystem'/Engine/Tutorial/SubEditors/TutorialAssets/TutorialParticleSystem.TutorialParticleSystem'"));
    if (ParticleAsset.Succeeded())
    {
        ImpactParticle = ParticleAsset.Object;
        UE_LOG(LogTemp, Log, TEXT("Successfully loaded ImpactParticle from code!"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load ImpactParticle in code! Check path."));
        ImpactParticle = nullptr;
    }*/
}

// Called when the game starts or when spawned
void AMySocketCultistActor::BeginPlay()
{
    Super::BeginPlay();
}

void AMySocketCultistActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void AMySocketCultistActor::SetClientSocket(SOCKET InSocket)
{
    ClientSocket = InSocket;
    if (ClientSocket != INVALID_SOCKET)
    {
        ReceiveData();
        UE_LOG(LogTemp, Log, TEXT("Cultist Client socket set. Starting ReceiveData."));

        auto packet_size = 3;

        TArray<uint8> PacketData;
        PacketData.SetNumUninitialized(packet_size);

        PacketData[0] = connectionHeader;
        PacketData[1] = packet_size;
        PacketData[2] = 0;

        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(PacketData.GetData()), PacketData.Num(), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendPlayerData failed with error: %ld"), WSAGetLastError());

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
                const int32 BufferSize = 1024;
                char Buffer[BufferSize];

                int32 BytesReceived = recv(ClientSocket, Buffer, BufferSize, 0);

                if (BytesReceived > 0)
                {
                    // 패킷의 첫 바이트는 헤더, 이후는 데이터
                    int PacketType = static_cast<int>(static_cast<unsigned char>(Buffer[0]));

                    switch (PacketType)
                    {
                    case cultistHeader:
                        ProcessCultistData(Buffer, BytesReceived);
                        break;
                    case policeHeader:
                        break;
                    case objectHeader:
                        //ProcessObjectData(Buffer, BytesReceived);
                        break;
                    case particleHeader:
                        //ProcessParticleData(Buffer, BytesReceived);
                        break;
                    case connectionHeader:
                        ProcessConnection(Buffer, BytesReceived);
                        break;
                    case DisconnectionHeader:
                        ProcessDisconnection(Buffer, BytesReceived);
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

void AMySocketCultistActor::ProcessCultistData(char* Buffer, int32 BytesReceived)
{
    if (BytesReceived < 2 + sizeof(FCultistCharacterState))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid Cultist Data Packet. BytesReceived: %d"), BytesReceived);
        return;
    }

    FCultistCharacterState ReceivedState;
    memcpy(&ReceivedState, Buffer + 2, sizeof(FCultistCharacterState));
    {
        FScopeLock Lock(&ReceivedDataMutex);
        ReceivedCultistStates.FindOrAdd(ReceivedState.PlayerID) = ReceivedState;
    }
    //else if (PacketType == policeHeader)
    //{
    //    while (Offset + sizeof(FPoliceCharacterState) <= BytesReceived) // 안전 체크
    //    {
    //        FPoliceCharacterState ReceivedState;
    //        memcpy(&ReceivedState, Buffer + Offset, sizeof(FPoliceCharacterState));

    //        {
    //            FScopeLock Lock(&ReceivedDataMutex);
    //            ReceivedPoliceStates.FindOrAdd(ReceivedState.PlayerID) = ReceivedState;
    //        }
    //        Offset += sizeof(FPoliceCharacterState);
    //    }
    //}
}

void AMySocketCultistActor::ProcessConnection(char* Buffer, int32 BytesReceived) {
    if (BytesReceived < 4)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid connection packet received."));
        return;
    }

    unsigned char packet_size = static_cast<unsigned char>(Buffer[1]);
    unsigned char connectedId = static_cast<unsigned char>(Buffer[2]);
    unsigned char role = static_cast<unsigned char>(Buffer[3]);
    
    if (my_ID == -1) {
        my_ID = static_cast<int>(connectedId);
        UE_LOG(LogTemp, Warning, TEXT("Connected. My ID is: %d"), my_ID);
    }
    else {
        TArray<char> BufferCopy;
        BufferCopy.SetNum(BytesReceived);
        memcpy(BufferCopy.GetData(), Buffer, BytesReceived);

        AsyncTask(ENamedThreads::GameThread, [this, BufferCopy, role]() mutable
            {
                if (role == 0) // Cultist
                {
                    this->SpawnCultistCharacter(BufferCopy.GetData());
                }
                else if (role == 1) // Police
                {
                    // this->SpawnPoliceCharacter(BufferCopy.GetData());
                }
            });
    }
}

void AMySocketCultistActor::ProcessDisconnection(char* Buffer, int32 BytesReceived)
{
    if (BytesReceived < 3) {
        UE_LOG(LogTemp, Error, TEXT("Invalid connection packet received."));
        return;
    }

    int DisconnectedID = static_cast<int>(static_cast<unsigned char>(Buffer[2]));

    if (DisconnectedID == my_ID)
    {
        CloseConnection();
        return;
    }

    SafeDestroyCharacter(DisconnectedID);
}

void AMySocketCultistActor::SendPlayerData()
{
    // 데이터 보내기
    if (ClientSocket != INVALID_SOCKET)
    {
        // 캐릭터 상태 가져오기
        ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
        if (PlayerCharacter)
        {
            FCultistCharacterState CharacterState = GetCharacterState(PlayerCharacter);

            auto packet_size = 3 + sizeof(FCultistCharacterState);

            TArray<uint8> PacketData;
            PacketData.SetNumUninitialized(packet_size);

            PacketData[0] = cultistHeader;
            PacketData[1] = packet_size;
            PacketData[2] = my_ID;
            memcpy(PacketData.GetData() + 3, &CharacterState, sizeof(FCultistCharacterState));

            int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(PacketData.GetData()), PacketData.Num(), 0);
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
}

FCultistCharacterState AMySocketCultistActor::GetCharacterState(ACharacter* PlayerCharacter)
{
    FCultistCharacterState State;
    State.PositionX = PlayerCharacter->GetActorLocation().X;
    State.PositionY = PlayerCharacter->GetActorLocation().Y;
    State.PositionZ = PlayerCharacter->GetActorLocation().Z;
    State.RotationPitch = PlayerCharacter->GetActorRotation().Pitch;
    State.RotationYaw = PlayerCharacter->GetActorRotation().Yaw;
    State.RotationRoll = PlayerCharacter->GetActorRotation().Roll;
    FVector Velocity = PlayerCharacter->GetVelocity();
    State.VelocityX = Velocity.X;
    State.VelocityY = Velocity.Y;
    State.VelocityZ = Velocity.Z;
    State.Speed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

    State.bIsCrouching = PlayerCharacter->bIsCrouched;

    ACultistCharacter* CultistChar = Cast<ACultistCharacter>(PlayerCharacter);
    if (CultistChar)
    {
        State.bIsPerformingRitual = CultistChar->bIsPerformingRitual;
        State.bIsHitByAnAttack = CultistChar->bIsHitByAnAttack;
        // UE_LOG(LogTemp, Error, TEXT("Client bIsPerformingRitual: %d, bIsHitByAnAttack: %d"), State.bIsPerformingRitual, State.bIsHitByAnAttack);
        State.CurrentHealth = CultistChar->CurrentHealth;
        // UE_LOG(LogTemp, Error, TEXT("health: %f"), State.CurrentHealth);
    }
    return State;
}

void AMySocketCultistActor::ProcessCharacterUpdates()
{
    FScopeLock Lock(&ReceivedDataMutex);

    for (auto& Pair : ReceivedCultistStates)
    {
        const FCultistCharacterState& State = Pair.Value;

        if (ACharacter* FoundChar = SpawnedCharacters.FindRef(Pair.Value.PlayerID))
        {
            UpdateCultistState(FoundChar, State);
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("No PlayerID %d"), Pair.Value.PlayerID);
        }
    }

    for (auto& Pair : ReceivedPoliceStates)
    {
        const FPoliceCharacterState& PoliceState = Pair.Value;

        if (ACharacter* FoundChar = SpawnedCharacters.FindRef(Pair.Value.PlayerID))
        {
            if (APoliceCharacter* PoliceChar = Cast<APoliceCharacter>(FoundChar))
            {
                // UpdatePoliceState(PoliceChar, PoliceState);
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

    if (State.bIsCrouching)
    {
        TargetLocation.Z += 50.0f;
    }

    FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, InterpSpeed);
    UE_LOG(LogTemp, Warning, TEXT("PlayerID %d - From (%f, %f, %f) To (%f, %f, %f)"),
        State.PlayerID,
        CurrentLocation.X, CurrentLocation.Y, CurrentLocation.Z,
        TargetLocation.X, TargetLocation.Y, TargetLocation.Z);
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
}

void AMySocketCultistActor::SpawnCultistCharacter(const char* Buffer)
{
    // PlayerID를 기반으로 고유 키 생성
    int PlayerID = static_cast<int>(static_cast<unsigned char>(Buffer[2]));
    // 이미 캐릭터가 존재하면 아무 작업도 하지 않음
    if (SpawnedCharacters.Contains(PlayerID))
    {
        UE_LOG(LogTemp, Warning, TEXT("Character already exists: %d"), PlayerID);
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    UClass* BP_ClientCharacter = LoadClass<ACharacter>(nullptr, TEXT("/Game/Cult_Custom/Characters/BP_Cultist_A_Client.BP_Cultist_A_Client_C"));

    if (BP_ClientCharacter)
    {
        ACharacter* NewCharacter = GetWorld()->SpawnActor<ACharacter>(
            BP_ClientCharacter,
            FVector(DummyState.PositionX, DummyState.PositionY, DummyState.PositionZ),
            FRotator(DummyState.RotationPitch, DummyState.RotationYaw, DummyState.RotationRoll),
            SpawnParams
        );
        if (NewCharacter)
        {
            SpawnedCharacters.Add(PlayerID, NewCharacter);
            ReceivedCultistStates.Add(PlayerID, DummyState);
            UE_LOG(LogTemp, Log, TEXT("Spawned new character for PlayerID=%d"), PlayerID);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn character for PlayerID=%d"), PlayerID);
        }
    }
}

void AMySocketCultistActor::CloseConnection() {
    if (ClientSocket != INVALID_SOCKET)
    {
        closesocket(ClientSocket);
        ClientSocket = INVALID_SOCKET;
    }
    WSACleanup();

    // 게임 종료
    // FGenericPlatformMisc::RequestExit(false);
    return;
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
void AMySocketClientActor::ProcessParticleData(char* Buffer, int32 BytesReceived) {
    int32 Offset = sizeof(uint8);

    if (BytesReceived < Offset + sizeof(FVector)) {
        UE_LOG(LogTemp, Error, TEXT("Invalid object data packet received."));
        return;
    }

    FVector NewVector;
    memcpy(&NewVector, Buffer + Offset, sizeof(FVector));
    ImpactLocations.Add(NewVector);
    UE_LOG(LogTemp, Log, TEXT("Added ImpactLocation: %s"), *NewVector.ToString());
    AsyncTask(ENamedThreads::GameThread, [this, NewVector]() {
        SpawnImpactEffect(NewVector);
        });
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
/*
void AMySocketClientActor::SpawnImpactEffect(const FVector& ImpactLocation)
{
    if (ImpactParticle)
    {
        UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(), ImpactParticle, ImpactLocation, FRotator::ZeroRotator, true);
        if (PSC)
        {
            PSC->bAutoDestroy = true;
            UE_LOG(LogTemp, Log, TEXT("Spawned Impact Effect at: %s"), *ImpactLocation.ToString());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ImpactParticle is not set."));
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

