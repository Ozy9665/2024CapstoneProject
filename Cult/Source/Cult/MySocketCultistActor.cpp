#include "MySocketCultistActor.h"
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// Sets default values
AMySocketCultistActor::AMySocketCultistActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

   // 파티클 코드
    ConstructorHelpers::FObjectFinder<UNiagaraSystem> NGParticleAsset(TEXT("/Game/MixedVFX/Particles/Explosions/NS_ExplosionMidAirSmall.NS_ExplosionMidAirSmall"));
    if (NGParticleAsset.Succeeded())
    {
        NG_ImpactParticle = NGParticleAsset.Object;
        UE_LOG(LogTemp, Log, TEXT("Successfully loaded NG_ImpactParticle!"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load NG_ImpactParticle! Check path."));
        NG_ImpactParticle = nullptr;
    }

    ConstructorHelpers::FObjectFinder<UNiagaraSystem> MuzzleAsset(TEXT("/Game/MixedVFX/Particles/Projectiles/Hits/NS_CursedArrow_Hit.NS_CursedArrow_Hit"));
    if (MuzzleAsset.Succeeded())
    {
        MuzzleImpactParticle = MuzzleAsset.Object;
        UE_LOG(LogTemp, Log, TEXT("Successfully loaded MuzzleImpactParticle!"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load MuzzleImpactParticle! Check path."));
        MuzzleImpactParticle = nullptr;
    }
}

// Called when the game starts or when spawned
void AMySocketCultistActor::BeginPlay()
{
    Super::BeginPlay();

    MyCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
    if (!MyCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("My character not found!"));
    }
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
                        ProcessPoliceData(Buffer, BytesReceived);
                        break;
                    case objectHeader:
                        //ProcessObjectData(Buffer, BytesReceived);
                        break;
                    case particleHeader:
                        ProcessParticleData(Buffer, BytesReceived);
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
        FScopeLock Lock(&CultistDataMutex);
        ReceivedCultistStates.FindOrAdd(ReceivedState.PlayerID) = ReceivedState;
    }
}

void AMySocketCultistActor::ProcessPoliceData(char* Buffer, int32 BytesReceived)
{
    if (BytesReceived < 2 + sizeof(FPoliceCharacterState))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid Police Data Packet. BytesReceived: %d"), BytesReceived);
        return;
    }

    FPoliceCharacterState ReceivedState;
    memcpy(&ReceivedState, Buffer + 2, sizeof(FPoliceCharacterState));
    {
        FScopeLock Lock(&PoliceDataMutex);
        ReceivedPoliceStates.FindOrAdd(ReceivedState.PlayerID) = ReceivedState;
    }
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
                    this->SpawnPoliceCharacter(BufferCopy.GetData());
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
        FCultistCharacterState CharacterState = GetCharacterState();

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
    State.bIsCrouching = MyCharacter->bIsCrouched;

    ACultistCharacter* CultistChar = Cast<ACultistCharacter>(MyCharacter);
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
    {
        FScopeLock Lock(&CultistDataMutex);
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
    }
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

    // ABP_IsNearToPakour 업데이트
    FProperty* ABP_IsNearToPakourProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsNearToPakour"));
    if (ABP_IsNearToPakourProperty && ABP_IsNearToPakourProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(ABP_IsNearToPakourProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, State.bIsNearEnoughToPakour);
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

void AMySocketCultistActor::SpawnPoliceCharacter(const char* Buffer)
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
}

void AMySocketCultistActor::ProcessParticleData(char* Buffer, int32 BytesReceived) {
    if (BytesReceived < 2 + sizeof(FImpactPacket))
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid Particle Data Packet. BytesReceived: %d"), BytesReceived);
        return;
    }

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
        GetWorld(), NG_ImpactParticle,
        ImpactPoint,
        ImpactRotation,
        FVector(1.0f), true, true,
        ENCPoolMethod::None, true
    );

    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        GetWorld(), MuzzleImpactParticle,
        MuzzleLoc,
        MuzzleRot
    );
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

