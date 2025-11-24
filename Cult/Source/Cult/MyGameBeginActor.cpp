// Fill out your copyright notice in the Description page of Project Settings.


#include "MyGameBeginActor.h"
#include "GameFramework/PlayerStart.h" 
#include "Kismet/GameplayStatics.h"
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
    GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) {
        UE_LOG(LogTemp, Error, TEXT("My Game Begin Actor GetGameInstance Failed!"));
    }
    SpawnActor();
}

void AMyGameBeginActor::SpawnActor() {
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (not PC) {
        UE_LOG(LogTemp, Log, TEXT("GetPlayerController Failed."));
        return;
    }

    if (GI->bIsCultist)
    {
        APawn* DefaultPawn = PC->GetPawn();
        if (not DefaultPawn)
        {
            UE_LOG(LogTemp, Log, TEXT("Default pawn is not a Police pawn or is null."));
            return;
        }
        if (!GI->CultistClass)
        {
            UE_LOG(LogTemp, Warning, TEXT("GI Class: %s"), *GI->GetClass()->GetName());

            UE_LOG(LogTemp, Error, TEXT("CultistClass in GI is null."));
            return;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        FVector SpawnLocation = DefaultPawn->GetActorLocation();
        FRotator SpawnRotation = DefaultPawn->GetActorRotation();
        APawn* CultistPawn = GetWorld()->SpawnActor<APawn>(GI->CultistClass, SpawnLocation, SpawnRotation, SpawnParams);
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
            CultistActor->SetClientSocket(GI->ClientSocket, GI->PendingRoomNumber);
            UE_LOG(LogTemp, Error, TEXT("Cultist Actor Spawned & Socket Passed."));
        }
    }
    else if (GI->bIsPolice)
    {
        APawn* DefaultPawn = PC->GetPawn();
        if (not DefaultPawn)
        {
            UE_LOG(LogTemp, Log, TEXT("Default pawn is not a Police pawn or is null."));
        }
        if (!GI->PoliceClass)
        {
            UE_LOG(LogTemp, Error, TEXT("PoliceClass in GI is null."));
            return;
        }


        FVector SpawnLocation = FVector::ZeroVector;
        FRotator SpawnRotation = FRotator::ZeroRotator;
        AActor* StartSpot = nullptr;
        TArray<AActor*> FoundStarts;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), FoundStarts);
        if (FoundStarts.Num() > 0)
        {
            // 첫 번째 PlayerStart를 사용
            StartSpot = FoundStarts[0];
            SpawnLocation = StartSpot->GetActorLocation();
            SpawnRotation = StartSpot->GetActorRotation();
            UE_LOG(LogTemp, Log, TEXT("Found PlayerStart! Spawning at: %s"), *SpawnLocation.ToString());
        }
        else
        {
            // PlayerStart가 없으면 기존대로 DefaultPawn
            UE_LOG(LogTemp, Warning, TEXT("No PlayerStart found. Using DefaultPawn location."));
            SpawnLocation = DefaultPawn->GetActorLocation();
            SpawnRotation = DefaultPawn->GetActorRotation();
        }
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;


        APawn* PolicePawn = GetWorld()->SpawnActor<APawn>(GI->PoliceClass, SpawnLocation, SpawnRotation, SpawnParams);
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
        // 개 위치
        if (GI->DogClass)
        {
            FVector DogLocation = PolicePawn->GetActorLocation()
                + (PolicePawn->GetActorRightVector() * 150.0f)
                + (PolicePawn->GetActorForwardVector() * -100.0f);

            FRotator DogRotation = PolicePawn->GetActorRotation();
            APawn* DogPawn = GetWorld()->SpawnActor<APawn>(GI->DogClass, DogLocation, DogRotation, SpawnParams);
            if (DogPawn)
            {
                UE_LOG(LogTemp, Log, TEXT("Spawned AI Police Dog"));
            }
        }

        if (not PoliceActor)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to spawn PoliceActor."));
            return;
        }
        PoliceActor->SetClientSocket(GI->ClientSocket, GI->PendingRoomNumber);
        UE_LOG(LogTemp, Error, TEXT("Cultist Actor Spawned & Socket Passed."));
    }

    Destroy();
}

// Called every frame
void AMyGameBeginActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

