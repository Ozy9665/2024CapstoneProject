// Fill out your copyright notice in the Description page of Project Settings.


#include "MyNetworkManagerActor.h"
#include "Engine/Engine.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Kismet/GameplayStatics.h>

#pragma comment(lib, "ws2_32.lib")  // Winsock 라이브러리 링크

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

AMyNetworkManagerActor::~AMyNetworkManagerActor()
{
    WSACleanup();
}

// Called when the game starts or when spawned
void AMyNetworkManagerActor::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Error, TEXT("Actor Spawned"));
    CheckAndSpawnActor();
}

// Called every frame
void AMyNetworkManagerActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

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

void AMyNetworkManagerActor::CheckAndSpawnActor()
{
    FString ServerIP = TEXT("127.0.0.1");  // 서버 IP 주소
    int32 ServerPort = 7777;               // 포트 번호

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);

    // 서버 연결 가능 여부 체크
    if (CanConnectToServer(ServerIP, ServerPort))
    {
        if (PC)
        {
            APawn* DefaultPawn = PC->GetPawn();
            if (DefaultPawn)
            {
                // 클라이언트라면 Cultist 폰으로 교체
                UClass* CultistClass = LoadClass<APawn>(nullptr,
                    TEXT("/Game/Cult_Custom/Characters/BP_Cultist_A.BP_Cultist_A_C"));
                if (CultistClass)
                {
                    FActorSpawnParameters SpawnParams;
                    SpawnParams.Owner = this;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                    FVector SpawnLocation = DefaultPawn->GetActorLocation();
                    FRotator SpawnRotation = DefaultPawn->GetActorRotation();
                    APawn* CultistPawn = GetWorld()->SpawnActor<APawn>(CultistClass, SpawnLocation, SpawnRotation, SpawnParams);
                    if (CultistPawn)
                    {
                        PC->Possess(CultistPawn);
                        DefaultPawn->Destroy();
                        UE_LOG(LogTemp, Log, TEXT("Spawned Cultist pawn and possessed it, default pawn destroyed."));
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("Failed to spawn Cultist pawn."));
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("Failed to load BP_Cultist_A class! Check path."));
                }
            }
            else
            {
                UE_LOG(LogTemp, Log, TEXT("Default pawn is not a Police pawn or is null."));
            }
        }
        else 
        {
            UE_LOG(LogTemp, Log, TEXT("GetPlayerController Failed."));
        }

        // 클라이언트 액터 스폰
        GetWorld()->SpawnActor<AMySocketClientActor>(AMySocketClientActor::StaticClass(), GetActorLocation(), GetActorRotation());
        UE_LOG(LogTemp, Error, TEXT("Client Actor Spawn"));
    }
    else
    {
        //if (PC)
        //{
        //    APawn* DefaultPawn = PC->GetPawn();
        //    if (DefaultPawn)
        //    {
        //        // 서버면 Police 폰으로 교체
        //        UClass* PoliceClass = LoadClass<APawn>(nullptr,
        //            TEXT("/Game/Cult_Custom/Characters/Police/BP_PoliceCharacter.BP_PoliceCharacter_C"));
        //        if (PoliceClass)
        //        {
        //            FActorSpawnParameters SpawnParams;
        //            SpawnParams.Owner = this;
        //            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        //            FVector SpawnLocation = DefaultPawn->GetActorLocation();
        //            FRotator SpawnRotation = DefaultPawn->GetActorRotation();
        //            APawn* PolicePawn = GetWorld()->SpawnActor<APawn>(PoliceClass, SpawnLocation, SpawnRotation, SpawnParams);
        //            if (PolicePawn)
        //            {
        //                PC->Possess(PolicePawn);
        //                DefaultPawn->Destroy();
        //                UE_LOG(LogTemp, Log, TEXT("Spawned Police pawn and possessed it, default pawn destroyed."));
        //            }
        //            else
        //            {
        //                UE_LOG(LogTemp, Error, TEXT("Failed to spawn Police pawn."));
        //            }
        //        }
        //        else
        //        {
        //            UE_LOG(LogTemp, Error, TEXT("Failed to load BP_Police class! Check path."));
        //        }
        //    }
        //    else
        //    {
        //        UE_LOG(LogTemp, Log, TEXT("Default pawn is not a Police pawn or is null."));
        //    }
        //}
        //else
        //{
        //    UE_LOG(LogTemp, Log, TEXT("GetPlayerController Failed."));
        //}

        // 서버 액터 스폰
        GetWorld()->SpawnActor<AMySocketActor>(AMySocketActor::StaticClass(), GetActorLocation(), GetActorRotation());
        UE_LOG(LogTemp, Error, TEXT("Server Actor Spawn"));
    }
    Destroy();
}