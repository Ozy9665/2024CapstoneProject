#include "AMyNetworkManagerActor.h"
#include "Engine/Engine.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <future>

#pragma comment(lib, "ws2_32.lib")  // Winsock 라이브러리 링크

// Sets default values
AMyNetworkManagerActor::AMyNetworkManagerActor()
{
    // Set this actor to call Tick() every frame. You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;

    // Initialize Winsock
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
    SpawnAppropriateActor();
}

AMyNetworkManagerActor::~AMyNetworkManagerActor()
{
    // Cleanup Winsock
    WSACleanup();
}

bool AMyNetworkManagerActor::CanConnectToServer(const FString& ServerIP, int32 ServerPort)
{
    SOCKET ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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
    bool bIsConnected = connect(ClientSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)) != SOCKET_ERROR;
    if (!bIsConnected)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to connect to server: %d"), WSAGetLastError());
    }

    closesocket(ClientSocket);
    return bIsConnected;
}

void AMyNetworkManagerActor::SpawnAppropriateActor()
{
    FString ServerIP = TEXT("127.0.0.1");  // 서버 IP 주소
    int32 ServerPort = 7777;               // 포트 번호

    // 비동기 소켓 연결 시도
    std::async(std::launch::async, [this, ServerIP, ServerPort]()
        {
            if (CanConnectToServer(ServerIP, ServerPort))
            {
                GetWorld()->SpawnActor<AMySocketClientActor>();  // 클라이언트 액터 스폰
            }
            else
            {
                GetWorld()->SpawnActor<AMySocketActor>();        // 서버 액터 스폰
            }
        });
}