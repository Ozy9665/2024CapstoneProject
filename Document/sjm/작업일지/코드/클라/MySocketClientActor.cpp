// Fill out your copyright notice in the Description page of Project Settings.


#include "MySocketClientActor.h"
#include "Engine/Engine.h"
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Kismet/GameplayStatics.h>
#include <GameFramework/Character.h>
#include "GameFramework/CharacterMovementComponent.h"
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

	FString ServerIP = TEXT("127.0.0.1");  // ���� IP
	int32 ServerPort = 7777;              // ���� ��Ʈ

    if (ConnectToServer(ServerIP, ServerPort))
    {
        UE_LOG(LogTemp, Log, TEXT("Connected to server!"));
        ReceiveData();  // ������ ���� ����
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to connect to server."));
    }
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
        UE_LOG(LogTemp, Error, TEXT("Failed to create socket: %ld"), WSAGetLastError());
        WSACleanup();
        return false;
    }

    sockaddr_in ServerAddr;
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(ServerPort);
    ServerAddr.sin_addr.s_addr = inet_addr(TCHAR_TO_ANSI(*ServerIP));

    if (connect(ClientSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)) == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("Connection failed with error: %ld"), WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return false;
    }

    return true;
}

void AMySocketClientActor::ReceiveData()
{
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
        {
            while (true)
            {
                char Buffer[sizeof(FCharacterState)];
                int32 BytesReceived = recv(ClientSocket, Buffer, sizeof(Buffer), 0);

                if (BytesReceived > 0)
                {
                    if (BytesReceived == sizeof(FCharacterState))
                    {
                        // ���ŵ� �����͸� FCharacterState�� ��ȯ
                        FCharacterState* ReceivedState = reinterpret_cast<FCharacterState*>(Buffer);

                        // ���� �����忡�� ĳ���� ������Ʈ
                        AsyncTask(ENamedThreads::GameThread, [this, ReceivedState]()
                            {
                                // �������� ���� ĳ���� ���� ���
                                UE_LOG(LogTemp, Log, TEXT("Received character state: PlayerID=(%d), Position=(%f, %f, %f), Rotation=(%f, %f, %f)"),
                                ReceivedState->PlayerID,
                                ReceivedState->PositionX, ReceivedState->PositionY, ReceivedState->PositionZ,
                                ReceivedState->RotationPitch, ReceivedState->RotationYaw, ReceivedState->RotationRoll);

                        // ĳ���� ������Ʈ ����
                        UpdateOrSpawnCharacter(*ReceivedState);
                            });
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Unexpected data size received: %d"), BytesReceived);
                    }
                }
                else if (BytesReceived == 0)
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

void AMySocketClientActor::UpdateOrSpawnCharacter(const FCharacterState& State)
{
    // PlayerID�� ������� ���� Ű ����
    FString CharacterKey = FString::Printf(TEXT("Character_%d"), State.PlayerID);

    // ���� ĳ���Ͱ� ������ ������Ʈ
    if (ACharacter* Character = SpawnedCharacters.FindRef(CharacterKey))
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
                    bool bShouldMove = (State.AnimationState == EAnimationState::Run);
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
        UE_LOG(LogTemp, Log, TEXT("Animation State: ShouldMove=%s, Velocity=(%f, %f, %f), GroundSpeed=%f, IsFalling=%s"),
            State.AnimationState == EAnimationState::Run ? TEXT("true") : TEXT("false"),
            State.VelocityX, State.VelocityY, State.VelocityZ,
            FVector(State.VelocityX, State.VelocityY, 0.0f).Size(),
            State.bIsFalling ? TEXT("true") : TEXT("false"));

    }
    else
    {
        // ���ο� ĳ���� ����
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
       // SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

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
}

void AMySocketClientActor::SendData()
{
    // ������ ������
    if (ClientSocket != INVALID_SOCKET)
    {
        // ĳ���� ���� ��������
        ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
        if (PlayerCharacter)
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
                CharacterState.AnimationState = EAnimationState::Idle; // �ӵ��� ���� 0�̸� Idle
            }
            else
            {
                CharacterState.AnimationState = EAnimationState::Run; // �̵� ���̸� Run
            }
            // ����ü�� ���̳ʸ� �����ͷ� ����
            char Buffer[sizeof(FCharacterState)];
            memcpy(Buffer, &CharacterState, sizeof(FCharacterState));

            int32 BytesSent = send(ClientSocket, Buffer, sizeof(FCharacterState), 0);

            if (BytesSent == SOCKET_ERROR)
            {
                UE_LOG(LogTemp, Error, TEXT("Send failed with error: %ld"), WSAGetLastError());
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("Sent character state: Position=(%f, %f, %f), Rotation=(%f, %f, %f)"),
                    CharacterState.PositionX, CharacterState.PositionY, CharacterState.PositionZ,
                    CharacterState.RotationPitch, CharacterState.RotationYaw, CharacterState.RotationRoll,
                    CharacterState.VelocityX, CharacterState.VelocityY, CharacterState.VelocityZ,
                    CharacterState.GroundSpeed,
                    CharacterState.bIsFalling ? TEXT("true") : TEXT("false"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("PlayerCharacter is null."));
        }
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

// Called every frame
void AMySocketClientActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    SendData();
}

