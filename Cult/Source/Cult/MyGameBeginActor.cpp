// Fill out your copyright notice in the Description page of Project Settings.


#include "MyGameBeginActor.h"
#include "Camera/CameraActor.h"

// Sets default values
AMyGameBeginActor::AMyGameBeginActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void AMyGameBeginActor::BeginPlay()
{
	Super::BeginPlay();
    SpawnActor();
}

void AMyGameBeginActor::SpawnActor() {
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
            CultistActor->SetClientSocket(MyGI->ClientSocket, MyGI->PendingRoomNumber);
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
        PoliceActor->SetClientSocket(MyGI->ClientSocket, MyGI->PendingRoomNumber);
        UE_LOG(LogTemp, Error, TEXT("Cultist Actor Spawned & Socket Passed."));
    }

    Destroy();
}





















// Called every frame
void AMyGameBeginActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

