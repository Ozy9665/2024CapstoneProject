// Fill out your copyright notice in the Description page of Project Settings.


#include "MySocketPoliceActor.h"

// Sets default values
AMySocketPoliceActor::AMySocketPoliceActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    // ��ƼŬ �ڵ�
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
void AMySocketPoliceActor::BeginPlay()
{
	Super::BeginPlay();
	
    MyCharacter = Cast<APoliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    if (!MyCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("My character not found!"));
    }
}

void AMySocketPoliceActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void AMySocketPoliceActor::SetClientSocket(SOCKET InSocket)
{
    ClientSocket = InSocket;
    if (ClientSocket != INVALID_SOCKET)
    {
        ReceiveData();
        UE_LOG(LogTemp, Log, TEXT("Police Client socket set. Starting ReceiveData."));

        auto packet_size = 3;

        TArray<uint8> PacketData;
        PacketData.SetNumUninitialized(packet_size);

        PacketData[0] = connectionHeader;
        PacketData[1] = packet_size;
        PacketData[2] = 1;

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
            while (true)
            {
                const int32 BufferSize = 1024;
                char Buffer[BufferSize];

                int32 BytesReceived = recv(ClientSocket, Buffer, BufferSize, 0);

                if (BytesReceived > 0)
                {
                    // ��Ŷ�� ù ����Ʈ�� ���, ���Ĵ� ������
                    int PacketType = static_cast<int>(static_cast<unsigned char>(Buffer[0]));

                    switch (PacketType)
                    {
                    case cultistHeader:
                        ProcessPlayerData(Buffer, BytesReceived);
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

void AMySocketPoliceActor::ProcessPlayerData(char* Buffer, int32 BytesReceived)
{
    if (BytesReceived < 2)
        return;
    int PacketType = static_cast<int>(static_cast<unsigned char>(Buffer[0]));
    int32 Offset = 2;

    while (Offset + sizeof(FCultistCharacterState) <= BytesReceived) // ���� üũ
    {
        FCultistCharacterState ReceivedState;
        memcpy(&ReceivedState, Buffer + Offset, sizeof(FCultistCharacterState));

        {
            FScopeLock Lock(&ReceivedDataMutex);
            ReceivedCultistStates.FindOrAdd(ReceivedState.PlayerID) = ReceivedState;
        }
        Offset += sizeof(FCultistCharacterState);
    }
}

void AMySocketPoliceActor::ProcessConnection(char* Buffer, int32 BytesReceived) {
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

void AMySocketPoliceActor::ProcessDisconnection(char* Buffer, int32 BytesReceived)
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

void AMySocketPoliceActor::SendPlayerData()
{
    // ������ ������
    if (ClientSocket != INVALID_SOCKET)
    {
        FPoliceCharacterState CharacterState = GetCharacterState();

        auto packet_size = 3 + sizeof(FPoliceCharacterState);

        TArray<uint8> PacketData;
        PacketData.SetNumUninitialized(packet_size);

        PacketData[0] = policeHeader;
        PacketData[1] = packet_size;
        PacketData[2] = my_ID;
        memcpy(PacketData.GetData() + 3, &CharacterState, sizeof(FPoliceCharacterState));

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

FPoliceCharacterState AMySocketPoliceActor::GetCharacterState()
{
    FPoliceCharacterState State;
    State.PlayerID = my_ID;

    // ��ġ �� ȸ�� ����
    State.PositionX = MyCharacter->GetActorLocation().X;
    State.PositionY = MyCharacter->GetActorLocation().Y;
    State.PositionZ = MyCharacter->GetActorLocation().Z;
    State.RotationPitch = MyCharacter->GetActorRotation().Pitch;
    State.RotationYaw = MyCharacter->GetActorRotation().Yaw;
    State.RotationRoll = MyCharacter->GetActorRotation().Roll;

    // �ӵ� �� Speed ���
    FVector Velocity = MyCharacter->GetVelocity();
    State.VelocityX = Velocity.X;
    State.VelocityY = Velocity.Y;
    State.VelocityZ = Velocity.Z;
    State.Speed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();

    // Crouch ����
    State.bIsCrouching = MyCharacter->bIsCrouched;

    // Aiming ����
    State.bIsAiming = MyCharacter->bIsAiming;

    // IsAttacking ����
    State.bIsAttacking = MyCharacter->bIsAttacking;

    // ���� ��
    State.CurrentWeapon = MyCharacter->CurrentWeapon;

    // ���� ���� ����
    State.bIsPakour = MyCharacter->IsPakour;
    State.bIsNearEnoughToPakour = MyCharacter->bIsNearEnoughToPakour;
    State.CurrentVaultType = MyCharacter->CurrentVaultType;

    // IsShooting ����
    State.bIsShooting = MyCharacter->bIsShooting;

    return State;
}

void AMySocketPoliceActor::SpawnCultistCharacter(const char* Buffer)
{
    // PlayerID�� ������� ���� Ű ����
    int PlayerID = static_cast<int>(static_cast<unsigned char>(Buffer[2]));
    // �̹� ĳ���Ͱ� �����ϸ� �ƹ� �۾��� ���� ����
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
            FVector(CultistDummyState.PositionX, CultistDummyState.PositionY, CultistDummyState.PositionZ),
            FRotator(CultistDummyState.RotationPitch, CultistDummyState.RotationYaw, CultistDummyState.RotationRoll),
            SpawnParams
        );
        if (NewCharacter)
        {
            SpawnedCharacters.Add(PlayerID, NewCharacter);
            ReceivedCultistStates.Add(PlayerID, CultistDummyState);
            UE_LOG(LogTemp, Log, TEXT("Spawned new character for PlayerID=%d"), PlayerID);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn character for PlayerID=%d"), PlayerID);
        }
    }
}

void AMySocketPoliceActor::ProcessCharacterUpdates(float DeltaTime)
{
    FScopeLock Lock(&ReceivedDataMutex);

    for (auto& Pair : ReceivedCultistStates)
    {
        const FCultistCharacterState& State = Pair.Value;

        if (ACharacter* FoundChar = SpawnedCharacters.FindRef(Pair.Value.PlayerID))
        {
            UpdateCultistState(FoundChar, State, DeltaTime);
        }
    }
}

void AMySocketPoliceActor::UpdateCultistState(ACharacter* Character, const FCultistCharacterState& State, float DeltaTime)
{
    float InterpSpeed = 30.0f; // ���� �ӵ�

    FVector CurrentLocation = Character->GetActorLocation();
    FVector TargetLocation(State.PositionX, State.PositionY, State.PositionZ);

    if (State.bIsCrouching)
    {
        TargetLocation.Z += 50.0f;
    }

    FVector NewLocation = FMath::VInterpTo(CurrentLocation, TargetLocation, DeltaTime, InterpSpeed);
    Character->SetActorLocation(NewLocation);
    Character->SetActorRotation(FRotator(State.RotationPitch, State.RotationYaw, State.RotationRoll));

    // �ִϸ��̼� ���� ������Ʈ
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

    // Speed ������Ʈ
    {
        FProperty* SpeedProperty = AnimInstance->GetClass()->FindPropertyByName(FName("Speed"));
        if (SpeedProperty && SpeedProperty->IsA<FDoubleProperty>())
        {
            FDoubleProperty* DoubleProp = CastFieldChecked<FDoubleProperty>(SpeedProperty);
            DoubleProp->SetPropertyValue_InContainer(AnimInstance, State.Speed);
        }
    }

    // IsCrouching ������Ʈ
    FProperty* IsCrouchingProperty = AnimInstance->GetClass()->FindPropertyByName(FName("Crouch"));
    if (IsCrouchingProperty && IsCrouchingProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsCrouchingProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, State.bIsCrouching);
    }
}

void AMySocketPoliceActor::CloseConnection() {
    if (ClientSocket != INVALID_SOCKET)
    {
        closesocket(ClientSocket);
        ClientSocket = INVALID_SOCKET;
    }
    WSACleanup();

    // ���� ����
    // FGenericPlatformMisc::RequestExit(false);
    return;
}

void AMySocketPoliceActor::SafeDestroyCharacter(int PlayerID)
{
    // �����ؼ� ���� ���
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

    // GameThread������ Destroy �ϵ���
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

// Called every frame
void AMySocketPoliceActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    SendPlayerData();
    // ProcessCharacterUpdates();
}

