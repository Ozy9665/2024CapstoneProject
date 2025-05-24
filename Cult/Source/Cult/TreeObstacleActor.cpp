// Fill out your copyright notice in the Description page of Project Settings.


#include "TreeObstacleActor.h"

// Sets default values
ATreeObstacleActor::ATreeObstacleActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	TreeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TreeMesh"));
	RootComponent = TreeMesh;

	TreeMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TreeMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TreeMesh->SetSimulatePhysics(false);

	// Spline 초기화
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
	Spline->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void ATreeObstacleActor::BeginPlay()
{
	Super::BeginPlay();
	
	InitialLocation = GetActorLocation();
	TargetLocation = InitialLocation + FVector(0.f, 0.f, GrowHeight);

	SetActorLocation(InitialLocation - FVector(0.f, 0.f, GrowHeight));
}

// Called every frame
void ATreeObstacleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsGrowing)
	{
		ElapsedTime += DeltaTime;
		float Alpha = FMath::Clamp(ElapsedTime / GrowTime, 0.f, 1.f);
		FVector NewLocation = FMath::Lerp(InitialLocation - FVector(0, 0, GrowHeight), InitialLocation, Alpha);
		SetActorLocation(NewLocation);

		if (Alpha >= 1.f)
		{
			bIsGrowing = false;
			SetupBranches();
		}
	}
}

void ATreeObstacleActor::SetupBranches()
{
	// 길이 방향 설정
	const float BranchLength = 200.f;

	TArray<FVector>Directions = {
		FVector(1,0,0),	// X
		FVector(-1,0,0),
		FVector(0,1,0),	// Y
		FVector(0,-1,0)
	};

	for (FVector Dir : Directions)
	{
		FVector Start = GetActorLocation();
		FVector End = Start + Dir * BranchLength;

		// spline 포인트 추가
		Spline->ClearSplinePoints(false);
		Spline->AddSplinePoint(Start, ESplineCoordinateSpace::World, false);
		Spline->AddSplinePoint(End, ESplineCoordinateSpace::World, true);

		// spline 메시생성
		USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);
		SplineMesh->RegisterComponent();
		SplineMesh->SetMobility(EComponentMobility::Movable);
		SplineMesh->SetStaticMesh(BranchMesh);
		SplineMesh->SetForwardAxis(ESplineMeshAxis::X);
		SplineMesh->AttachToComponent(Spline, FAttachmentTransformRules::KeepRelativeTransform);

		FVector StartPos, StartTangent, EndPos, EndTangent;
		Spline->GetLocalLocationAndTangentAtSplinePoint(0, StartPos, StartTangent);
		Spline->GetLocalLocationAndTangentAtSplinePoint(1, EndPos, EndTangent);

		SplineMesh->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent);
		SpawnedBranches.Add(SplineMesh);
	}

}