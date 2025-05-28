// Fill out your copyright notice in the Description page of Project Settings.


#include "ProceduralBranchActor.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"

// Sets default values
AProceduralBranchActor::AProceduralBranchActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProcMesh"));
	SetRootComponent(ProcMesh);
	ProcMesh->bUseComplexAsSimpleCollision = false;
	ProcMesh->SetSimulatePhysics(true);
}

// Called when the game starts or when spawned
void AProceduralBranchActor::BeginPlay()
{
	Super::BeginPlay();
	

	GenerateCurvedBranch();
}

// Called every frame
void AProceduralBranchActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AProceduralBranchActor::GenerateCurvedBranch(float Length, float RadiusStart, float RadiusEnd, int32 Segments)
{
	TArray<FVector> SplinePoints;
	for (int32 i = 0; i <= Segments; ++i)
	{
		float T = (float)i / Segments;
		float Z = FMath::Lerp(0.f, Length, T);
		float XOffset = FMath::FRandRange(-10.f, 10.f);
		float YOffset = FMath::FRandRange(-10.f, 10.f);
		SplinePoints.Add(FVector(Z, YOffset, XOffset));
	}
	GenerateCylinderAlongSpline(SplinePoints, RadiusStart, RadiusEnd);
}

void AProceduralBranchActor::GenerateCylinderAlongSpline(const TArray<FVector>& SplinePoints, float RadiusStart, float RadiusEnd)
{
	const int32 NumSides = 8; // 원형 단면 분할 수
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;

	int32 VertexCount = 0;
	const int32 NumSegments = SplinePoints.Num() - 1;

	for (int32 i = 0; i < SplinePoints.Num(); ++i)
	{
		const FVector& Current = SplinePoints[i];
		const FVector Forward = (i == SplinePoints.Num() - 1) ? (Current - SplinePoints[i - 1]) : (SplinePoints[i + 1] - Current);
		const FVector Tangent = Forward.GetSafeNormal();
		const FVector Up = FVector::UpVector;
		const FVector Right = FVector::CrossProduct(Up, Tangent).GetSafeNormal();
		const FVector TrueUp = FVector::CrossProduct(Tangent, Right).GetSafeNormal();

		// 현재 단면에서 반지름 보간
		float Alpha = (float)i / (SplinePoints.Num() - 1);
		float Radius = FMath::Lerp(RadiusStart, RadiusEnd, Alpha);

		for (int32 j = 0; j < NumSides; ++j)
		{
			float Angle = 2 * PI * ((float)j / NumSides);
			FVector RadialDir = FMath::Cos(Angle) * Right + FMath::Sin(Angle) * TrueUp;
			FVector Vertex = Current + Radius * RadialDir;

			Vertices.Add(Vertex);
			Normals.Add(RadialDir);
			UVs.Add(FVector2D((float)j / NumSides, Alpha));
		}
	}

	// 삼각형 연결
	for (int32 i = 0; i < NumSegments; ++i)
	{
		for (int32 j = 0; j < NumSides; ++j)
		{
			int32 CurrStart = i * NumSides + j;
			int32 CurrEnd = i * NumSides + (j + 1) % NumSides;
			int32 NextStart = (i + 1) * NumSides + j;
			int32 NextEnd = (i + 1) * NumSides + (j + 1) % NumSides;

			// 두 삼각형으로 사각형 
			Triangles.Add(CurrStart);
			Triangles.Add(NextEnd);
			Triangles.Add(NextStart);

			Triangles.Add(CurrStart);
			Triangles.Add(CurrEnd);
			Triangles.Add(NextEnd);
		}
	}

	ProcMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, {}, {}, true);
	ProcMesh->SetMaterial(0, ProcMesh->GetMaterial(0)); // 머티리얼 유지
}