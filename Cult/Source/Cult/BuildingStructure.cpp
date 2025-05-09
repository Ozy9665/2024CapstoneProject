// Fill out your copyright notice in the Description page of Project Settings.


#include "BuildingStructure.h"
#include "DrawDebugHelpers.h"

// Sets default values
ABuildingStructure::ABuildingStructure()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ABuildingStructure::BeginPlay()
{
	Super::BeginPlay();
	
	InitializeStructure(5, 5, 5);

	// 충격 테스트 - 중앙블록에 충격
	int32 MiddleIndex = GetBlockIndex(2, 2, 0, 5, 5, 5);
	ApplyImpulseToBlock(MiddleIndex, FVector(0, 0, 200.f));
}

// Called every frame
void ABuildingStructure::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (int32 i = 0; i < Blocks.Num(); ++i)
	{
		if (!Blocks[i].bIsCollapsed && Blocks[i].AccumulatedImpulse >= Blocks[i].Integrity)
		{
			ApplyImpulseToBlock(i, FVector(0, 0, Blocks[i].AccumulatedImpulse));
		}
	}


	DebugDrawStructure();
}

void ABuildingStructure::InitializeStructure(int32 Width, int32 Height, int32 Depth)
{
	Blocks.Empty();
	const float BlockSpacing = 100.f;

	// 블록 생성
	for (int32 x = 0; x < Width; ++x)
	{
		for (int32 y = 0; y < Height; ++y)
		{
			for (int32 z = 0; z < Depth; ++z)
			{
				FBuildingBlock Block;
				Block.Position = FVector(x * BlockSpacing, y * BlockSpacing, z * BlockSpacing);
				Blocks.Add(Block);
			}
		}
	}

	// 연결 설정
	for (int32 x = 0; x < Width; ++x)
	{
		for (int32 y = 0; y < Height; ++y)
		{
			for (int32 z = 0; z < Depth; ++z)
			{
				int32 Index = GetBlockIndex(x, y, z, Width, Height, Depth);
				TArray<int32>& Connected = Blocks[Index].ConnectedIndices;

				// 주변 연결
				TArray<FIntVector> Neighbors = {
					 {x + 1, y, z}, {x - 1, y, z}, {x, y + 1, z}, {x, y - 1, z}, {x, y, z + 1}, {x, y, z - 1}
				};

				for (const FIntVector& N : Neighbors)
				{
					if (N.X >= 0 && N.X < Width &&
						N.Y >= 0 && N.Y < Height &&
						N.Z >= 0 && N.Z < Depth)
					{
						Connected.Add(GetBlockIndex(N.X, N.Y, N.Z, Width, Height, Depth));
					}
				}
			}
		}
	}
}

int32 ABuildingStructure::GetBlockIndex(int32 X, int32 Y, int32 Z, int32 Width, int32 Height, int32 Depth) const
{
	return X * Height * Depth + Y * Depth + Z;
}


void ABuildingStructure::DebugDrawStructure()
{
	for (const FBuildingBlock& Block : Blocks)
	{
		FColor Color = Block.bIsCollapsed ? FColor::Red : FColor::Green;
		FVector WorldPos = GetActorLocation() + Block.Position;
		DrawDebugBox(GetWorld(), WorldPos, FVector(40), Color, false, -1.f, 0, 2.f);
	}
}

void ABuildingStructure::ApplyImpulseToBlock(int32 BlockIndex, const FVector& Impulse)
{
	if (!Blocks.IsValidIndex(BlockIndex)) return;

	FBuildingBlock& Block = Blocks[BlockIndex];
	Block.AccumulatedImpulse += Impulse.Size();

	// 임계치 넘었을 시 붕괴
	if (!Block.bIsCollapsed && Block.AccumulatedImpulse >= Block.Integrity)
	{
		Block.bIsCollapsed = true;

		// 인접 블록 충격 전달
		for (int32 NeighborIdx : Block.ConnectedIndices)
		{
			if (Blocks.IsValidIndex(NeighborIdx) && !Blocks[NeighborIdx].bIsCollapsed)
			{
				Blocks[NeighborIdx].AccumulatedImpulse += Block.AccumulatedImpulse * 0.5f;
			}
		}
	}
}