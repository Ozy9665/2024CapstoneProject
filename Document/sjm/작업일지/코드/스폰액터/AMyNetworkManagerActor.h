#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MySocketActor.h"
#include "MySocketClientActor.h"
#include "AMyNetworkManagerActor.generated.h"

UCLASS()
class YOURGAME_API AMyNetworkManagerActor : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    AMyNetworkManagerActor();

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

private:
    // Attempts to connect to the server and returns true if successful
    bool CanConnectToServer(const FString& ServerIP, int32 ServerPort);

    // Spawns the appropriate actor based on server availability
    void SpawnAppropriateActor();
};
