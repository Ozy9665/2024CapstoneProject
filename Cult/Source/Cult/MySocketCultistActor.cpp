#include "MySocketCultistActor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Camera/CameraActor.h"
#include "CultistCharacter.h"
#include "Components/TextBlock.h"

#pragma comment(lib, "ws2_32.lib")
AMySocketCultistActor* MySocketCultistActor = nullptr;

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
    MySocketCultistActor = this;
    MyCharacter = Cast<ACultistCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
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

        ConnectionPacket packet;
        packet.header = connectionHeader;
        packet.size = sizeof(ConnectionPacket);
        packet.role = 0;
        
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&packet), sizeof(ConnectionPacket), 0);
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
            const int32 BufferSize = 1024;
            while (true)
            {
                char Buffer[BufferSize];
                int32 BytesReceived = recv(ClientSocket, Buffer, BufferSize, 0);
                if (BytesReceived > 0)
                {
                    int PacketType = static_cast<int>(static_cast<unsigned char>(Buffer[0]));
                    int PacketSize = static_cast<int>(static_cast<unsigned char>(Buffer[1]));
                    if (BytesReceived != PacketSize)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Invalid packet size: Received %d, Expected %d: header:%d"), BytesReceived, PacketSize, PacketType);
                        continue;
                    }

                    switch (PacketType)
                    {
                    case cultistHeader:
                        ProcessCultistData(Buffer);
                        break;
                    case objectHeader:
                        //ProcessObjectData(Buffer);
                        break;
                    case policeHeader:
                        ProcessPoliceData(Buffer);
                        break;
                    case particleHeader:
                        ProcessParticleData(Buffer);
                        break;
                    case hitHeader:
                        ProcessHitData(Buffer);
                        break;
                    case connectionHeader:
                        ProcessConnection(Buffer);
                        break;
                    case DisconnectionHeader:
                        ProcessDisconnection(Buffer);
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

void AMySocketCultistActor::ProcessCultistData(const char* Buffer)
{
    FCultistCharacterState ReceivedState;
    memcpy(&ReceivedState, Buffer + 2, sizeof(FCultistCharacterState));
    {
        FScopeLock Lock(&CultistDataMutex);
        ReceivedCultistStates.FindOrAdd(ReceivedState.PlayerID) = ReceivedState;
    }
}

void AMySocketCultistActor::ProcessPoliceData(const char* Buffer)
{
    FPoliceCharacterState ReceivedState;
    memcpy(&ReceivedState, Buffer + 2, sizeof(FPoliceCharacterState));
    {
        FScopeLock Lock(&PoliceDataMutex);
        ReceivedPoliceStates.FindOrAdd(ReceivedState.PlayerID) = ReceivedState;
    }
}

void AMySocketCultistActor::ProcessHitData(const char* Buffer)
{
    FHitPacket ReceivedState;
    memcpy(&ReceivedState, Buffer + 2, sizeof(FHitPacket));
    {
        FScopeLock Lock(&CultistDataMutex);
        switch (ReceivedState.Weapon)
        {
        case EWeaponType::Baton:
        {
            APoliceCharacter* Attacker = Cast<APoliceCharacter>(SpawnedCharacters.FindRef(ReceivedState.AttackerID));
            if (not Attacker) {
                UE_LOG(LogTemp, Error, TEXT("ProcessHitData failed with error spawn attacker: %d"), ReceivedState.AttackerID);
                return;
            }
            if (ReceivedState.TargetID == my_ID)
            {
                if (not MyCharacter)
                {
                    UE_LOG(LogTemp, Error, TEXT("MyCharacter is null for self-hit!"));
                    return;
                }
                AsyncTask(ENamedThreads::GameThread, [this, Attacker]()
                    {
                        MyCharacter->OnHitbyBaton(Attacker->GetActorLocation(), BatonAttackDamage);
                    });
            }
            else {
                ACultistCharacter* Target = Cast<ACultistCharacter>(SpawnedCharacters.FindRef(ReceivedState.TargetID));
                if (not Target) {
                    UE_LOG(LogTemp, Error, TEXT("ProcessHitData failed with error spawn Target: %d"), ReceivedState.TargetID);
                    return;
                }
                AsyncTask(ENamedThreads::GameThread, [this, Target, Attacker]()
                    {
                        Target->OnHitbyBaton(Attacker->GetActorLocation(), BatonAttackDamage);
                    });
            }

            break;
        }
        case EWeaponType::Pistol: 
        {
            if (ReceivedState.TargetID == my_ID)
            {
                if (not MyCharacter)
                {
                    UE_LOG(LogTemp, Error, TEXT("MyCharacter is null for self-hit!"));
                    return;
                }
                AsyncTask(ENamedThreads::GameThread, [this]()
                    {
                        MyCharacter->TakeDamage(PistolAttackDamage);
                    });
            }
            else {
                ACultistCharacter* Target = Cast<ACultistCharacter>(SpawnedCharacters.FindRef(ReceivedState.TargetID));
                if (not Target) {
                    UE_LOG(LogTemp, Error, TEXT("ProcessHitData failed with error spawn Target: %d"), ReceivedState.TargetID);
                    return;
                }
                AsyncTask(ENamedThreads::GameThread, [this, Target]()
                    {
                        Target->TakeDamage(PistolAttackDamage);
                    });
            }
            UE_LOG(LogTemp, Error, TEXT("EWeaponType received: %d"), ReceivedState.Weapon);
            break;
        }
        case EWeaponType::Taser:
        {
            APoliceCharacter* Attacker = Cast<APoliceCharacter>(SpawnedCharacters.FindRef(ReceivedState.AttackerID));
            if (not Attacker) {
                UE_LOG(LogTemp, Error, TEXT("ProcessHitData failed with error spawn attacker: %d"), ReceivedState.AttackerID);
                return;
            }
            if (ReceivedState.TargetID == my_ID)
            {
                if (not MyCharacter)
                {
                    UE_LOG(LogTemp, Error, TEXT("MyCharacter is null for self-hit!"));
                    return;
                }
                AsyncTask(ENamedThreads::GameThread, [this, Attacker]()
                    {
                        MyCharacter->GotHitTaser(Attacker);
                    });
            }
            else {
                ACultistCharacter* Target = Cast<ACultistCharacter>(SpawnedCharacters.FindRef(ReceivedState.TargetID));
                if (not Target) {
                    UE_LOG(LogTemp, Error, TEXT("ProcessHitData failed with error spawn Target: %d"), ReceivedState.TargetID);
                    return;
                }
                AsyncTask(ENamedThreads::GameThread, [this, Target, Attacker]()
                    {
                        Target->GotHitTaser(Attacker);
                    });
            }
            break;
        }
        default:
            UE_LOG(LogTemp, Error, TEXT("EWeaponType Error: %d"), ReceivedState.Weapon);
            break;
        }
    }
}

void AMySocketCultistActor::ProcessConnection(const char* Buffer) {
    unsigned char connectedId = static_cast<unsigned char>(Buffer[2]);
    unsigned char role = static_cast<unsigned char>(Buffer[3]);
    
    if (my_ID == -1) {
        my_ID = static_cast<int>(connectedId);
        UE_LOG(LogTemp, Warning, TEXT("Connected. My ID is: %d"), my_ID);

        if (MyCharacter) {
            MyCharacter->my_ID = my_ID;
        }
    }
    else {
        AsyncTask(ENamedThreads::GameThread, [this, connectedId, role]() mutable
            {
                if (role == 0) // Cultist
                {
                    this->SpawnCultistCharacter(connectedId);
                }
                else if (role == 1) // Police
                {
                    this->SpawnPoliceCharacter(connectedId);
                }
                else if (role == 99) { //PoliceAi
                    this->SpawnPoliceAICharacter(connectedId);
                }
            });
    }
}

void AMySocketCultistActor::ProcessDisconnection(const char* Buffer)
{
    int DisconnectedID = static_cast<int>(static_cast<unsigned char>(Buffer[2]));
    if (DisconnectedID == my_ID)
    {
        CloseConnection();
    }
    else {
        SafeDestroyCharacter(DisconnectedID);
    }
}

void AMySocketCultistActor::SendDisable() 
{
    if (ClientSocket != INVALID_SOCKET)
    {
        DisablePakcet Packet;
        Packet.header = disableHeader;
        Packet.size = sizeof(DisablePakcet);
        Packet.id = my_ID;
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(DisablePakcet), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendDisable failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CloseConnection();
    }
}

void AMySocketCultistActor::SendPlayerData()
{
    if (ClientSocket != INVALID_SOCKET)
    {
        CultistPacket Packet;
        Packet.header = cultistHeader;
        Packet.size = sizeof(CultistPacket);
        Packet.state = GetCharacterState();

        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(CultistPacket), 0);
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
    State.Crouch = MyCharacter->bIsCrouched;

    ACultistCharacter* CultistChar = Cast<ACultistCharacter>(MyCharacter);
    if (CultistChar)
    {
        State.ABP_IsPerforming = CultistChar->bIsPerformingRitual;
        State.ABP_IsHitByAnAttack = CultistChar->bIsHitByAnAttack;
        State.ABP_IsFrontKO = CultistChar->bIsFrontFallen;
        State.ABP_IsElectric = CultistChar->bIsElectric;
        State.ABP_TTStun = CultistChar->TurnToStun;
        State.ABP_TTGetUp = CultistChar->TurnToGetUp;
        State.ABP_IsDead = CultistChar->bIsDead;
        State.ABP_IsStunned = CultistChar->bIsStunned;
        
        //UE_LOG(LogTemp, Error, TEXT("Client ABP_TTStun: %d, ABP_IsDead: %d"), State.ABP_TTStun, CultistChar->bIsDead);
        State.CurrentHealth = CultistChar->CurrentHealth;
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
                if (ACultistCharacter* CultistChar = Cast<ACultistCharacter>(FoundChar)) 
                {
                    UpdateCultistState(CultistChar, State);
                }
            }
            else {
                UE_LOG(LogTemp, Warning, TEXT("No Cultist PlayerID %d"), Pair.Value.PlayerID);
                KeysToRemove.Add(Pair.Value.PlayerID);
            }
        }
    }   
    for (int32 Key : KeysToRemove)
    {
        ReceivedCultistStates.Remove(Key);
    }
    KeysToRemove.Reset();
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
                else {
                    UE_LOG(LogTemp, Warning, TEXT("No Police PlayerID %d"), Pair.Value.PlayerID);
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
}

void AMySocketCultistActor::SpawnCultistCharacter(const unsigned char PlayerID)
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
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    UClass* BP_ClientCharacter = LoadClass<ACharacter>(nullptr, TEXT("/Game/Cult_Custom/Characters/BP_Cultist_A_Client.BP_Cultist_A_Client_C"));

    if (BP_ClientCharacter)
    {
        ACultistCharacter* NewCharacter = GetWorld()->SpawnActor<ACultistCharacter>(
            BP_ClientCharacter,
            FVector(CultistDummyState.PositionX, CultistDummyState.PositionY, CultistDummyState.PositionZ),
            FRotator(CultistDummyState.RotationPitch, CultistDummyState.RotationYaw, CultistDummyState.RotationRoll),
            SpawnParams
        );
        if (NewCharacter)
        {
            SpawnedCharacters.Add(PlayerID, NewCharacter);
            ReceivedCultistStates.Add(PlayerID, CultistDummyState);
            UE_LOG(LogTemp, Log, TEXT("Spawned new cultist character for PlayerID=%d"), PlayerID);

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

void AMySocketCultistActor::SpawnPoliceCharacter(const unsigned char PlayerID)
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
            UE_LOG(LogTemp, Log, TEXT("Spawned new police character for PlayerID=%d"), PlayerID);

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
}

void AMySocketCultistActor::SpawnPoliceAICharacter(const unsigned char PlayerID)
{
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
    // ready Header send
    uint8 PacketData = readyHeader;
    int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&PacketData), sizeof(PacketData), 0);
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendReadyPacket failed with error: %ld"), WSAGetLastError());
    }
}

void AMySocketCultistActor::ProcessParticleData(char* Buffer) {
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
    closesocket(ClientSocket);
    ClientSocket = INVALID_SOCKET;
    WSACleanup();

    AsyncTask(ENamedThreads::GameThread, [this]()
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

            // 게임 종료
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

