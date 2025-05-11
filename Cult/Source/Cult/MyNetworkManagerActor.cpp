// Fill out your copyright notice in the Description page of Project Settings.


#include "MyNetworkManagerActor.h"
#include "Engine/Engine.h"
#include "Camera/CameraActor.h"
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

SOCKET AMyNetworkManagerActor::CanConnectToServer(const FString& ServerIP, int32 ServerPort)
{
    SOCKET ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ClientSocket == INVALID_SOCKET)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create socket: %d"), WSAGetLastError());
        return INVALID_SOCKET;
    }

    sockaddr_in ServerAddr;
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(ServerPort);
    ServerAddr.sin_addr.s_addr = inet_addr(TCHAR_TO_ANSI(*ServerIP));

    // Attempt to connect
    
    if (connect(ClientSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)) != SOCKET_ERROR)
    {
        UE_LOG(LogTemp, Log, TEXT("Connected to server."));
        return ClientSocket;
    }
    else {
        UE_LOG(LogTemp, Error, TEXT("Failed to connect to server: %d"), WSAGetLastError());
        closesocket(ClientSocket);
        return INVALID_SOCKET;
    }
}

void AMyNetworkManagerActor::CheckAndSpawnActor()
{
    FString ServerIP = TEXT("127.0.0.1");  // 서버 IP 주소
    int32 ServerPort = 7777;               // 포트 번호

    SOCKET ConnectedSocket = CanConnectToServer(ServerIP, ServerPort);
    if (ConnectedSocket == INVALID_SOCKET)
    {
        UE_LOG(LogTemp, Error, TEXT("Server Is Closed."));
        return;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (not PC) {
        UE_LOG(LogTemp, Log, TEXT("GetPlayerController Failed."));
        return;
    }

    UMyGameInstance* MyGI = Cast<UMyGameInstance>(GetGameInstance());
    if (not MyGI) {
        UE_LOG(LogTemp, Error, TEXT("GameInstance 캐스팅 실패."));
        return;
    }

    if (MyGI->bIsCultist) 
    {
        APawn* DefaultPawn = PC->GetPawn();
        if (not DefaultPawn)
        {
            UE_LOG(LogTemp, Log, TEXT("Default pawn is not a Police pawn or is null."));
            return;
        }

        UClass* CultistClass = LoadClass<APawn>(nullptr,
            TEXT("/Game/Cult_Custom/Characters/BP_Cultist_A.BP_Cultist_A_C"));
        if (not CultistClass)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to load BP_Cultist_A class"));
            return;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        FVector SpawnLocation = DefaultPawn->GetActorLocation();
        FRotator SpawnRotation = DefaultPawn->GetActorRotation();
        APawn* CultistPawn = GetWorld()->SpawnActor<APawn>(CultistClass, SpawnLocation, SpawnRotation, SpawnParams);
        if (not CultistPawn)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn Cultist pawn."));
            return;
        }
        PC->Possess(CultistPawn);
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
        UChildActorComponent* CAC = CultistPawn->FindComponentByClass<UChildActorComponent>();
        if (not CAC)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to FindComponentByClass"));
            return;
        }
        AActor* Spawned = CAC->GetChildActor();
        if (not Spawned)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to GetChildActor"));
            return;
        }
        ACameraActor* CamActor = Cast<ACameraActor>(Spawned);
        if (CamActor)
        {
            PC->SetViewTargetWithBlend(CamActor, 0.f);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ChildActor wasn't a CameraActor"));
            return;
        }

        DefaultPawn->Destroy();
        UE_LOG(LogTemp, Log, TEXT("Spawned Cultist pawn and possessed it, default pawn destroyed."));

        // 컬트 액터 스폰
        AMySocketCultistActor* CultistActor = GetWorld()->SpawnActor<AMySocketCultistActor>(
            AMySocketCultistActor::StaticClass(), GetActorLocation(), GetActorRotation());
        if (CultistActor)
        {
            CultistActor->SetClientSocket(ConnectedSocket);
            UE_LOG(LogTemp, Error, TEXT("Cultist Actor Spawned & Socket Passed."));
        }
    }
    else if (MyGI->bIsPolice)
    {
        APawn* DefaultPawn = PC->GetPawn();
        if (not DefaultPawn)
        {
            UE_LOG(LogTemp, Log, TEXT("Default pawn is not a Police pawn or is null."));
        }

        UClass* PoliceClass = LoadClass<APawn>(nullptr,
            TEXT("/Game/Cult_Custom/Characters/Police/BP_PoliceCharacter.BP_PoliceCharacter_C"));
        if (not PoliceClass)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to load BP_Police class! Check path."));
            return;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        FVector SpawnLocation = DefaultPawn->GetActorLocation();
        FRotator SpawnRotation = DefaultPawn->GetActorRotation();
        APawn* PolicePawn = GetWorld()->SpawnActor<APawn>(PoliceClass, SpawnLocation, SpawnRotation, SpawnParams);
        if (not PolicePawn)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn Police pawn."));
            return;
        }
        PC->Possess(PolicePawn);
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
        UChildActorComponent* CAC = PolicePawn->FindComponentByClass<UChildActorComponent>();
        if (not CAC)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to FindComponentByClass"));
            return;
        }
        AActor* Spawned = CAC->GetChildActor();
        if (not Spawned)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to GetChildActor"));
            return;
        }
        ACameraActor* CamActor = Cast<ACameraActor>(Spawned);
        if (CamActor)
        {
            PC->SetViewTargetWithBlend(CamActor, 0.f);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ChildActor wasn't a CameraActor"));
            return;
        }

        DefaultPawn->Destroy();
        UE_LOG(LogTemp, Log, TEXT("Spawned Police pawn and possessed it, default pawn destroyed."));

        AMySocketPoliceActor* PoliceActor = GetWorld()->SpawnActor<AMySocketPoliceActor>(
            AMySocketPoliceActor::StaticClass(), GetActorLocation(), GetActorRotation());
        if (not PoliceActor)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn PoliceActor."));
            return;
        }
        PoliceActor->SetClientSocket(ConnectedSocket);
        UE_LOG(LogTemp, Error, TEXT("Cultist Actor Spawned & Socket Passed."));
    }
    
    
    Destroy();
}