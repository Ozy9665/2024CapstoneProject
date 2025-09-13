// Fill out your copyright notice in the Description page of Project Settings.


#include "MyNetworkManagerActor.h"
#include "Engine/Engine.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Kismet/GameplayStatics.h>

#pragma comment(lib, "ws2_32.lib")  // Winsock ���̺귯�� ��ũ

// Sets default values
AMyNetworkManagerActor::AMyNetworkManagerActor()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;

    WSADATA WsaData;
    int Result = WSAStartup(MAKEWORD(2, 2), &WsaData);
    if (Result != 0)
    {
        UE_LOG(LogTemp, Error, TEXT("WSAStartup failed: %d"), Result);
    }
}

// Called when the game starts or when spawned
void AMyNetworkManagerActor::BeginPlay()
{
    Super::BeginPlay();
    GI = Cast<UMyGameInstance>(GetGameInstance());
    if (not GI) {
        UE_LOG(LogTemp, Error, TEXT("Gl cast failed."));
        return;
    }
    UE_LOG(LogTemp, Error, TEXT("Actor Spawned"));
    CheckServer();
}

// Called every frame
void AMyNetworkManagerActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

bool AMyNetworkManagerActor::CanConnectToServer(const FString& ServerIP, int32 ServerPort)
{
    ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ClientSocket == INVALID_SOCKET)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create socket: %d"), WSAGetLastError());
        return false;
    }

    sockaddr_in ServerAddr;
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(ServerPort);
    ServerAddr.sin_addr.s_addr = inet_addr(TCHAR_TO_ANSI(*ServerIP));

    // Attempt to connect
    
    if (connect(ClientSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)) != SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Log, TEXT("Connected to server."));
        return true;
    }
    else {
        UE_LOG(LogTemp, Error, TEXT("Failed to connect to server: %d"), WSAGetLastError());
        closesocket(ClientSocket);
        return false;
    }
}

void AMyNetworkManagerActor::CheckServer()
{
    FString ServerIP = TEXT("127.0.0.1");  // ���� IP �ּ�
    int32 ServerPort = 7777;               // ��Ʈ ��ȣ

    if(!CanConnectToServer(ServerIP, ServerPort))
    {
        UE_LOG(LogTemp, Error, TEXT("Server Is Closed."));
        return;
        // ���� ���� ���н� ���� ����� ����
    }

    ReceiveData();
    GI->ClientSocket = ClientSocket;
    IdOnlyPacket packet;
    packet.header = connectionHeader;
    packet.size = sizeof(IdOnlyPacket);

    int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&packet), sizeof(IdOnlyPacket), 0);
    if (BytesSent == SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Error, TEXT("SetClientSocket failed with error: %ld"), WSAGetLastError());
    }
}

void AMyNetworkManagerActor::TryLogin() {
    if (ClientSocket != INVALID_SOCKET)
    {
        ReceiveData();
        IdPacket Packet;
        Packet.header = loginHeader;
        Packet.size = sizeof(IdPacket);
        FMemory::Memzero(Packet.id, sizeof(Packet.id));
        FTCHARToUTF8 IdUtf8(*GI->Id);
        FCStringAnsi::Strncpy(Packet.id, IdUtf8.Get(), sizeof(Packet.id) - 1); // -1
        Packet.id[sizeof(Packet.id) - 1] = '\0'; // �������� �� ����

        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(IdPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("RequestRoomInfo failed with error: %ld"), WSAGetLastError());
        }
        else {
            UE_LOG(LogTemp, Warning, TEXT("send success, bytes = %d"), BytesSent);
        }
    }
    else
    {
        CheckServer();
    }
}

void AMyNetworkManagerActor::CheckIDValidation() {
    if (ClientSocket != INVALID_SOCKET)
    {
        ReceiveData();
        IdPacket Packet;
        Packet.header = idExistHeader;
        Packet.size = sizeof(IdPacket);
        FMemory::Memzero(Packet.id, sizeof(Packet.id));
        FTCHARToUTF8 IdUtf8(*GI->Id);
        FCStringAnsi::Strncpy(Packet.id, IdUtf8.Get(), sizeof(Packet.id) - 1); // -1
        Packet.id[sizeof(Packet.id) - 1] = '\0'; // �������� �� ����

        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(IdPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("CheckIDValidation failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CheckServer();
    }
}

void AMyNetworkManagerActor::TrySignUp() {
    if (ClientSocket != INVALID_SOCKET)
    {
        ReceiveData();
        IdPwPacket Packet;
        Packet.header = signUpHeader;
        Packet.size = sizeof(IdPwPacket);
        FMemory::Memzero(Packet.id, sizeof(Packet.id));
        FTCHARToUTF8 IdUtf8(*GI->Id);
        FCStringAnsi::Strncpy(Packet.id, IdUtf8.Get(), sizeof(Packet.id) - 1);
        Packet.id[sizeof(Packet.id) - 1] = '\0';

        FTCHARToUTF8 PwUtf8(*GI->Password);
        FCStringAnsi::Strncpy(Packet.pw, PwUtf8.Get(), sizeof(Packet.pw) - 1);
        Packet.pw[sizeof(Packet.pw) - 1] = '\0';

        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(IdPwPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("TrySignUp failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CheckServer();
    }
}

void AMyNetworkManagerActor::RequestRoomInfo() 
{
    if (ClientSocket != INVALID_SOCKET)
    {
        ReceiveData();
        RoleOnlyPacket Packet;
        Packet.header = requestHeader;
        Packet.size = sizeof(RoleOnlyPacket);
        Packet.role = GI->bIsCultist ? 0 : 1;
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(RoleOnlyPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("RequestRoomInfo failed with error: %ld"), WSAGetLastError());
        }
    }
    else
    {
        CheckServer();
    }
}

void AMyNetworkManagerActor::ProcessRoomInfo(const char* Buffer) 
{
    RoomsPakcet Packet;
    memcpy(&Packet, Buffer, sizeof(RoomsPakcet));

    rooms.Empty();
    rooms.Reserve(10);

    for (int i = 0; i < 10; ++i)
    {
        const PacketRoom& pr = Packet.rooms[i];
            
        Froom fr;
        fr.room_id = pr.room_id;
        fr.police = pr.police;
        fr.cultist = pr.cultist;
        fr.isIngame = pr.isIngame;

        fr.player_ids.Empty();
        fr.player_ids.Reserve(5);
        for (int j = 0; j < 5; ++j)
            fr.player_ids.Add(pr.player_ids[j]);

        rooms.Add(MoveTemp(fr));
    }
    AsyncTask(ENamedThreads::GameThread, [this]()
        {
            OnRoomListUpdated.Broadcast();
        });
}

void AMyNetworkManagerActor::SendEnterPacket() {
    if (ClientSocket != INVALID_SOCKET)
    {
        ReceiveData();
        RoomNumberPacket Packet;
        Packet.header = enterHeader;
        Packet.size = sizeof(RoomNumberPacket);
        Packet.room_number = GI->PendingRoomNumber;
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(RoomNumberPacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendEnterPacket failed with error: %ld"), WSAGetLastError());
        }
    }
}

void AMyNetworkManagerActor::RequestRitualData() {
    if (ClientSocket != INVALID_SOCKET)
    {
        ReceiveData();
        NoticePacket Packet;
        Packet.header = ritualHeader;
        Packet.size = sizeof(NoticePacket);
        int32 BytesSent = send(ClientSocket, reinterpret_cast<const char*>(&Packet), sizeof(NoticePacket), 0);
        if (BytesSent == SOCKET_ERROR)
        {
            UE_LOG(LogTemp, Error, TEXT("SendEnterPacket failed with error: %ld"), WSAGetLastError());
        }
    }
}

void AMyNetworkManagerActor::ProcessRitualData(const char* Buffer)
{
    RitualPacket Packet;
    memcpy(&Packet, Buffer, sizeof(RitualPacket));

    AsyncTask(ENamedThreads::GameThread, [this, Packet]() {
        GI->RutialSpawnLocations.Empty();
        GI->RutialSpawnLocations = {
            Packet.Loc1,
            Packet.Loc2,
            Packet.Loc3
        };
        for (int32 i = 0; i < GI->RutialSpawnLocations.Num(); ++i)
        {
            const FVector& Loc = GI->RutialSpawnLocations[i];
            UE_LOG(LogTemp, Warning, TEXT("RitualLocation[%d]: %s"), i, *Loc.ToString());
        }
        UE_LOG(LogTemp, Warning, TEXT("OnGameStartConfirmed: Ritual Saved."));
        OnGameStartConfirmed.Broadcast();
    });
}

void AMyNetworkManagerActor::ReceiveData() 
{
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]() {
        char Buffer[BufferSize];
        int32 BytesReceived = recv(ClientSocket, Buffer, BufferSize, 0);
        if (BytesReceived > 0)
        {
            int PacketType = static_cast<int>(static_cast<unsigned char>(Buffer[0]));
            int PacketSize = static_cast<int>(static_cast<unsigned char>(Buffer[1]));
            if (BytesReceived != PacketSize)
            {
                UE_LOG(LogTemp, Warning, TEXT("Invalid packet size: Received %d, Expected %d: header:%d"), BytesReceived, PacketSize, PacketType);
                return;
            }
            switch (PacketType)
            {
            case connectionHeader:
            {
                my_ID = static_cast<unsigned char>(Buffer[2]);
                break;
            }
            case requestHeader:
            {
                ProcessRoomInfo(Buffer);
                break;
            }
            case enterHeader: 
            {
                RequestRitualData();
                break;
            }
            case leaveHeader: 
            {
                AsyncTask(ENamedThreads::GameThread, [this]() {
                    OnGameStartUnConfirmed.Broadcast();
                    UE_LOG(LogTemp, Warning, TEXT("OnGameStartUnConfirmed"));
                    });
                break;
            }
            case ritualHeader:
            {
                ProcessRitualData(Buffer);
                break;
            }
            case loginHeader:
            {
                BoolPacket* p = reinterpret_cast<BoolPacket*>(Buffer);
                bool bSuccess = p->result;
                AsyncTask(ENamedThreads::GameThread, [this, bSuccess]() {
                    if (bSuccess)
                    {
                        OnLoginSuccess.Broadcast();
                        UE_LOG(LogTemp, Warning, TEXT("Login Success"));
                    }
                    else
                    {
                        OnLoginFailed.Broadcast();
                        UE_LOG(LogTemp, Warning, TEXT("Login Failed"));
                    }
                    });
                break;
            }
            case idExistHeader:
            {
                BoolPacket* p = reinterpret_cast<BoolPacket*>(Buffer);
                bool bSuccess = p->result;
                AsyncTask(ENamedThreads::GameThread, [this, bSuccess]() {
                    if (bSuccess)
                    {
                        OnIdIsExist.Broadcast();
                        UE_LOG(LogTemp, Warning, TEXT("Id already Exist"));
                    }
                    else
                    {
                        GI->canUseId = true;
                        OnIdIsNotExist.Broadcast();
                        UE_LOG(LogTemp, Warning, TEXT("Can sign up"));
                    }
                    });
                break;
            }
            case signUpHeader:
            {
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
            CheckServer();
            return;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("recv failed with error: %ld"), WSAGetLastError());
            CheckServer();
            return;
        }
        });
}
