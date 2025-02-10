// Fill out your copyright notice in the Description page of Project Settings.


#include "AMyNetworkManagerActor.h"
#include "Engine/Engine.h"
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")  // Winsock 라이브러리 링크

// Sets default values
AAMyNetworkManagerActor::AAMyNetworkManagerActor()
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

AAMyNetworkManagerActor::~AAMyNetworkManagerActor()
{
    WSACleanup();
}

// Called when the game starts or when spawned
void AAMyNetworkManagerActor::BeginPlay()
{
	Super::BeginPlay();
    UE_LOG(LogTemp, Error, TEXT("Actor Spawned"));
    CheckAndSpawnActor();
}

// Called every frame
void AAMyNetworkManagerActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AAMyNetworkManagerActor::CanConnectToServer(const FString& ServerIP, int32 ServerPort)
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

void AAMyNetworkManagerActor::CheckAndSpawnActor()
{
    FString ServerIP = TEXT("127.0.0.1");  // 서버 IP 주소
    int32 ServerPort = 7777;               // 포트 번호

    // 서버 연결 가능 여부 체크
    if (CanConnectToServer(ServerIP, ServerPort))
    {
        // 클라이언트 액터 스폰
        GetWorld()->SpawnActor<AMySocketClientActor>(AMySocketClientActor::StaticClass(), GetActorLocation(), GetActorRotation());
        UE_LOG(LogTemp, Error, TEXT("Client Actor Spawn"));
    }
    else
    {
        // 서버 액터 스폰
        GetWorld()->SpawnActor<AMySocketActor>(AMySocketActor::StaticClass(), GetActorLocation(), GetActorRotation());
        UE_LOG(LogTemp, Error, TEXT("Server Actor Spawn"));
    }
    Destroy();
}