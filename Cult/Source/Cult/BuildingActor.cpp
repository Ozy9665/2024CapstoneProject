// Fill out your copyright notice in the Description page of Project Settings.


#include "BuildingActor.h"
#include "Components/SceneComponent.h"

// Sets default values
ABuildingActor::ABuildingActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

}

// Called when the game starts or when spawned
void ABuildingActor::BeginPlay()
{
	Super::BeginPlay();
	GenerateBuilding();
	ConnectNeighbors();
}

// Called every frame
void ABuildingActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABuildingActor::GenerateBuilding()
{
	Blocks.Empty();

    for (int32 Z = 0; Z < Depth; Z++)
    {
        for (int32 Y = 0; Y < Height; Y++)
        {
            for (int32 X = 0; X < Width; X++)
            {
                FVector Location = FVector(X * BlockSpacing, Y * BlockSpacing, Z * BlockSpacing);
                FString Name = FString::Printf(TEXT("Block_%d_%d_%d"), X, Y, Z);

                UBuildingBlockComponent* NewBlock = NewObject<UBuildingBlockComponent>(this, BlockComponentClass, *Name);
                if (NewBlock)
                {
                    NewBlock->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
                    NewBlock->RegisterComponent();
                    NewBlock->SetRelativeLocation(Location);

                    Blocks.Add(NewBlock);
                }
            }
        }
    }
}

void ABuildingActor::ConnectNeighbors()
{
    for (int32 Z = 0; Z < Depth; Z++)
    {
        for (int32 Y = 0; Y < Height; Y++)
        {
            for (int32 X = 0; X < Width; X++)
            {
                int32 Index = GetIndex(X, Y, Z);
                if (!Blocks.IsValidIndex(Index)) continue;

                UBuildingBlockComponent* Block = Blocks[Index];
                if (!Block) continue;

                // 인접한 6방향 블록 연결
                TArray<FIntVector> Offsets = {
                    {1, 0, 0}, {-1, 0, 0},
                    {0, 1, 0}, {0, -1, 0},
                    {0, 0, 1}, {0, 0, -1}
                };

                for (const FIntVector& Offset : Offsets)
                {
                    int32 NX = X + Offset.X;
                    int32 NY = Y + Offset.Y;
                    int32 NZ = Z + Offset.Z;
                    int32 NeighborIndex = GetIndex(NX, NY, NZ);

                    if (Blocks.IsValidIndex(NeighborIndex))
                    {
                        Block->ConnectedBlocks.Add(Blocks[NeighborIndex]);
                    }
                }
            }
        }
    }
}

int32 ABuildingActor::GetIndex(int32 X, int32 Y, int32 Z) const
{
    if (X < 0 || Y < 0 || Z < 0 || X >= Width || Y >= Height || Z >= Depth)
        return -1;
    return X + Y * Width + Z * Width * Height;
}