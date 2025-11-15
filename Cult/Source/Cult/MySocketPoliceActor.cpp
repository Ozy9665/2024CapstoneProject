// Fill out your copyright notice in the Description page of Project Settings.


#include "MySocketPoliceActor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Camera/CameraActor.h"
#include "PoliceCharacter.h"
#include "Components/TextBlock.h"
#include "TreeObstacleActor.h"
#include "CrowActor.h"

#pragma comment(lib, "ws2_32.lib")
AMySocketPoliceActor* MySocketPoliceActor = nullptr;

// Sets default values
AMySocketPoliceActor::AMySocketPoliceActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AMySocketPoliceActor::BeginPlay()
{
	Super::BeginPlay();
    MySocketPoliceActor = this;
    MyCharacter = Cast<APoliceCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    if (!MyCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("My character not found!"));
    }
    GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) {
        UE_LOG(LogTemp, Error, TEXT("My Socket Police Actor GetGameInstance Failed!"));
    }
}

void AMySocketPoliceActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (ClientSocket != INVALID_SOCKET)
    {
        SendDisconnection();
        closesocket(ClientSocket);
        ClientSocket = INVALID_SOCKET;
    }

    WSACleanup();
    UE_LOG(LogTemp, Log, TEXT("Client socket closed and cleaned up."));
}

void AMySocketPoliceActor::SetClientSocket(SOCKET InSocket, int32 RoomNumber)
{
    ClientSocket = InSocket;
    if (ClientSocket != INVALID_SOCKET)
    {
        ReceiveData();
        UE_LOG(LogTemp, Log, TEXT("Police Client socket set. Starting ReceiveData."));

        RoomNumberPacket packet;
        packet.header = gameStartHeader;
        packet.size = sizeof(RoomNumberPacket);
        packet.room_number = RoomNumber;

        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&packet), sizeof(RoomNumberPacket), 0);
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
                char Buffer[BufferSize];
                int32 BytesReceived = recv(ClientSocket, Buffer, BufferSize, 0);
                if (BytesReceived > 0)
                {
                    int PacketType = static_cast<int>(static_cast<unsigned char>(Buffer[0]));
                    int PacketSize = static_cast<int>(static_cast<unsigned char>(Buffer[1]));
                    if (BytesReceived != PacketSize)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Invalid packet size: Received %d, Expected %d"), BytesReceived, PacketSize);
                        continue;
                    }

                    switch (PacketType)
                    {
                    case cultistHeader:
                        ProcessPlayerData(Buffer);
                        break;
                    case treeHeader:
                        ProcessSkillData(Buffer);
                        break;
                    case particleHeader:
                        //ProcessParticleData(Buffer);
                        break;
                    case hitHeader:
                        //ProcessHitData(Buffer);
                        break;
                    case connectionHeader:
                        ProcessConnection(Buffer);
                        break;
                    case DisconnectionHeader:
                        ProcessDisconnection(Buffer);
                        break;
                    case disappearHeader:
                    {
                        unsigned char id = static_cast<unsigned char>(Buffer[2]);
                        HideCharacter(id, true);
                        break;
                    }
                    case appearHeader:
                    {
                        unsigned char id = static_cast<unsigned char>(Buffer[2]);
                        HideCharacter(id, false);
                        break;
                    }
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

void AMySocketPoliceActor::ProcessPlayerData(const char* Buffer)
{
    FCultistCharacterState ReceivedState;
    memcpy(&ReceivedState, Buffer + 2, sizeof(FCultistCharacterState));
    {
        FScopeLock Lock(&ReceivedDataMutex);
        ReceivedCultistStates.FindOrAdd(ReceivedState.PlayerID) = ReceivedState;
    }
}

void AMySocketPoliceActor::ProcessHitData(const char* Buffer)
{
    FHitPacket ReceivedState;
    memcpy(&ReceivedState, Buffer + 2, sizeof(FHitPacket));
    {
        FScopeLock Lock(&ReceivedDataMutex);
        switch (ReceivedState.Weapon)
        {
        case EWeaponType::Baton:
            break;
        case EWeaponType::Pistol:
            UE_LOG(LogTemp, Error, TEXT("EWeaponType received: %d"), ReceivedState.Weapon);
            break;
        case EWeaponType::Taser:
            UE_LOG(LogTemp, Error, TEXT("EWeaponType received: %d"), ReceivedState.Weapon);
            break;
        default:
            UE_LOG(LogTemp, Error, TEXT("EWeaponType Error: %d"), ReceivedState.Weapon);
            break;
        }
    }
}

void AMySocketPoliceActor::ProcessSkillData(const char* Buffer) 
{
    TreePacket ReceivedSkill;
    memcpy(&ReceivedSkill, Buffer, sizeof(TreePacket));

    const int Key = static_cast<int>(ReceivedSkill.casterId);
    ACharacter* FoundChar = SpawnedCharacters.FindRef(Key);
    if (!FoundChar) {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] caster %d not found"), ReceivedSkill.casterId);
        return;
    }

    ACultistCharacter* CasterCultist = Cast<ACultistCharacter>(FoundChar);
    if (!CasterCultist) {
        UE_LOG(LogTemp, Warning, TEXT("[Skill] caster %d is not Cultist"), ReceivedSkill.casterId);
        return;
    }

    TWeakObjectPtr<ACultistCharacter> WeakCaster = CasterCultist;
    const FVector  SpawnLoc = AMySocketActor::ToUE(ReceivedSkill.SpawnLoc);
    const FRotator SpawnRot = AMySocketActor::ToUE(ReceivedSkill.SpawnRot);
    const uint8    Skill = ReceivedSkill.skill;

    AsyncTask(ENamedThreads::GameThread, [WeakCaster, SpawnLoc, SpawnRot, Skill]() {
        ACultistCharacter* Caster = WeakCaster.Get();
        if (!IsValid(Caster)) return;

        FActorSpawnParameters Params;
        Params.Owner = Caster;
        Params.Instigator = Caster;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        switch (Skill)
        {
        case 1:
        {
            if (!Caster->TreeObstacleActorClass) {
                UE_LOG(LogTemp, Error, TEXT("[Skill] %s TreeObstacleActorClass is null"), *Caster->GetName());
                break;
            }
            Caster->GetWorld()->SpawnActor<ATreeObstacleActor>(
                Caster->TreeObstacleActorClass, SpawnLoc, SpawnRot, Params);
            break;
        }
        case 2:
        {
            if (!Caster->CrowClass) {
                UE_LOG(LogTemp, Error, TEXT("[Skill] %s CrowClass is null"), *Caster->GetName());
                break;
            }
            Caster->CrowInstance = Caster->GetWorld()->SpawnActor<ACrowActor>(
                Caster->CrowClass, SpawnLoc, SpawnRot, Params);
            if (Caster->CrowInstance) {
                Caster->CrowInstance->InitCrow(Caster, Caster->CrowLifetime);
            }
            break;
        }
        default:
            UE_LOG(LogTemp, Error, TEXT("Unknown Skill Number: %d"), Skill);
            break;
        }
        });
}

void AMySocketPoliceActor::ProcessConnection(const char* Buffer) {
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
                    UE_LOG(LogTemp, Warning, TEXT("ProcessConnection called with role 1. ID:%d"), my_ID);
                    // this->SpawnPoliceCharacter(connectedId);
                }
            });
    }
}

void AMySocketPoliceActor::ProcessDisconnection(const char* Buffer)
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

void AMySocketPoliceActor::SendPlayerData()
{
    if (ClientSocket != INVALID_SOCKET)
    {
        PolicePacket Packet;
        Packet.header = policeHeader;
        Packet.size = sizeof(PolicePacket);
        Packet.state = GetCharacterState();
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(PolicePacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendPlayerData failed with error: %ld"), WSAGetLastError());
        }
        SendDogData();
    }
    else
    {
        CloseConnection();
    }
}

void AMySocketPoliceActor::SendDogData() {
    if (ClientSocket != INVALID_SOCKET)
    {
        DogPacket Packet;
        Packet.header = dogHeader;
        Packet.size = sizeof(DogPacket);
        Packet.dog = GetDog();
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(DogPacket), 0);
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

void AMySocketPoliceActor::SendHitData(FHitPacket hitPacket) {
    if (ClientSocket != INVALID_SOCKET)
    {
        HitPacket Packet;
        Packet.header = hitHeader;
        Packet.size = sizeof(HitPacket);
        Packet.data = hitPacket;
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(HitPacket), 0);
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

    // Aiming 상태
    State.bIsAiming = MyCharacter->bIsAiming;

    // IsAttacking 상태
    State.bIsAttacking = MyCharacter->bIsAttacking;

    // 무기 값
    State.CurrentWeapon = MyCharacter->CurrentWeapon;

    // 파쿠르 관련 상태
    State.bIsPakour = MyCharacter->IsPakour;
    State.CurrentVaultType = MyCharacter->CurrentVaultType;
    // IsShooting 상태
    State.bIsShooting = MyCharacter->bIsShooting;

    return State;
}

Dog AMySocketPoliceActor::GetDog() {
    Dog dog;
    dog.loc;
    dog.rot;
    dog.owner = MyCharacter->my_ID;

    return dog;
}

void AMySocketPoliceActor::SpawnCultistCharacter(const unsigned char PlayerID)
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
    if (!GI || !GI->CultistClientClass)
    {
        UE_LOG(LogTemp, Error, TEXT("GI or PoliceClientClass is null"));
        return;
    }
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    ACharacter* NewCharacter = GetWorld()->SpawnActor<ACharacter>(
        GI->CultistClientClass,
        FVector(CultistDummyState.PositionX, CultistDummyState.PositionY, CultistDummyState.PositionZ),
        FRotator(CultistDummyState.RotationPitch, CultistDummyState.RotationYaw, CultistDummyState.RotationRoll),
        SpawnParams
    );
    if (NewCharacter)
    {
        SpawnedCharacters.Add(PlayerID, NewCharacter);
        ReceivedCultistStates.Add(PlayerID, CultistDummyState);
        UE_LOG(LogTemp, Log, TEXT("Spawned new character for PlayerID=%d"), PlayerID);

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

void AMySocketPoliceActor::ProcessCharacterUpdates()
{
    FScopeLock Lock(&ReceivedDataMutex);
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
            UE_LOG(LogTemp, Warning, TEXT("No PlayerID %d"), Pair.Value.PlayerID);
            KeysToRemove.Add(Pair.Value.PlayerID);
        }
    }
    for (int32 Key : KeysToRemove)
    {
        ReceivedCultistStates.Remove(Key);
    }
    KeysToRemove.Reset();
}

void AMySocketPoliceActor::UpdateCultistState(ACharacter* Character, const FCultistCharacterState& State)
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

void AMySocketPoliceActor::UpdateCultistAnimInstanceProperties(UAnimInstance* AnimInstance, const FCultistCharacterState& State)
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

    FProperty* IsABP_IsPakourProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_IsPakour"));
    if (IsABP_IsPakourProperty && IsABP_IsPakourProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_IsPakourProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.bIsPakour));
    }

    FProperty* IsABP_DoHealProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_DoHeal"));
    if (IsABP_DoHealProperty && IsABP_DoHealProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_DoHealProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.ABP_DoHeal));
    }

    FProperty* IsABP_GetHealProperty = AnimInstance->GetClass()->FindPropertyByName(FName("ABP_GetHeal"));
    if (IsABP_GetHealProperty && IsABP_GetHealProperty->IsA<FBoolProperty>())
    {
        FBoolProperty* BoolProp = CastFieldChecked<FBoolProperty>(IsABP_GetHealProperty);
        BoolProp->SetPropertyValue_InContainer(AnimInstance, static_cast<bool>(State.ABP_GetHeal));
    }
}

void AMySocketPoliceActor::CheckImpactEffect()
{
    // ImpactLocations.Add(MyCharacter->ParticleResult.ImpactPoint);
    UE_LOG(LogTemp, Log, TEXT("Added ImpactLocation: %s"), *MyCharacter->ParticleResult.ImpactPoint.ToString());
    SpawnImpactEffect(MyCharacter->ParticleResult);
    if (ClientSocket != INVALID_SOCKET)
    {
        SendParticleData(MyCharacter->ParticleResult);
    }
    else
    {
        CloseConnection();
    }
    MyCharacter->bHit = false;
    
}

void AMySocketPoliceActor::SpawnImpactEffect(FHitResult HitResult) {
    if (GI->NGParticleAsset)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), GI->NGParticleAsset,
            HitResult.ImpactPoint,
            HitResult.ImpactNormal.Rotation(),
            FVector(1.0f), true, true,
            ENCPoolMethod::None, true
        );
    }

    if (GI->MuzzleEffect)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            GetWorld(), GI->MuzzleEffect,
            MyCharacter->MuzzleLocation->GetComponentLocation(),
            MyCharacter->MuzzleLocation->GetComponentRotation()
        );
    }
}

void AMySocketPoliceActor::SendParticleData(FHitResult HitResult) {
    FVector MuzzleLoc = MyCharacter->MuzzleLocation->GetComponentLocation();
    FRotator MuzzleRot = MyCharacter->MuzzleLocation->GetComponentRotation();

    ParticlePacket Packet;
    Packet.header = particleHeader;
    Packet.size = sizeof(ParticlePacket);
    Packet.data.ImpactX = HitResult.ImpactPoint.X;
    Packet.data.ImpactY = HitResult.ImpactPoint.Y;
    Packet.data.ImpactZ = HitResult.ImpactPoint.Z;
    Packet.data.NormalX = HitResult.ImpactNormal.X;
    Packet.data.NormalY = HitResult.ImpactNormal.Y;
    Packet.data.NormalZ = HitResult.ImpactNormal.Z;
    Packet.data.MuzzleX = MuzzleLoc.X;
    Packet.data.MuzzleY = MuzzleLoc.Y;
    Packet.data.MuzzleZ = MuzzleLoc.Z;
    Packet.data.MuzzlePitch = MuzzleRot.Pitch;
    Packet.data.MuzzleYaw = MuzzleRot.Yaw;
    Packet.data.MuzzleRoll = MuzzleRot.Roll;

    int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(ParticlePacket), 0);
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendParticleData failed with error: %ld"), WSAGetLastError());
    }
}

void AMySocketPoliceActor::HideCharacter(int PlayerID, bool bHide) {
    AsyncTask(ENamedThreads::GameThread, [this, PlayerID, bHide]()
        {
            if (ACharacter* Char = SpawnedCharacters.FindRef(PlayerID)) {
                Char->SetActorHiddenInGame(bHide);
                Char->SetActorEnableCollision(!bHide);
                Char->SetActorTickEnabled(!bHide);

                UE_LOG(LogTemp, Log, TEXT("Character %d %s"),
                    PlayerID, bHide ? TEXT("hidden") : TEXT("unhidden"));
            }
        });
}

void AMySocketPoliceActor::SendDisconnection() {
    NoticePacket Packet;
    Packet.header = DisconnectionHeader;
    Packet.size = sizeof(NoticePacket);

    int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(NoticePacket), 0);
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SendDisconnection failed with error: %ld"), WSAGetLastError());
    }
}

void AMySocketPoliceActor::CloseConnection() {
    closesocket(ClientSocket);
    ClientSocket = INVALID_SOCKET;
    WSACleanup();

    AsyncTask(ENamedThreads::GameThread, [this]()
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);

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

void AMySocketPoliceActor::SafeDestroyCharacter(int PlayerID)
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

const TMap<int, ACharacter*>& AMySocketPoliceActor::GetSpawnedCharacters() const
{
    return SpawnedCharacters;
}



// Called every frame
void AMySocketPoliceActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    SendPlayerData();
    ProcessCharacterUpdates();
    // ProcessObjectUpdates(DeltaTime);
}

