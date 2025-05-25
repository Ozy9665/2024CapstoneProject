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
	Spline->SetMobility(EComponentMobility::Movable);
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
	UE_LOG(LogTemp, Warning, TEXT(">>> SetupBranches CALLED"));

	if (!BranchMesh || !Spline)
	{
		UE_LOG(LogTemp, Warning, TEXT("BranchMesh or Spline is nullptr"));
		return;
	}

	Spline->SetMobility(EComponentMobility::Movable);
	Spline->ClearSplinePoints();
	Spline->AddSplinePoint(FVector(0.f, 0.f, 0.f), ESplineCoordinateSpace::Local, false);
	Spline->AddSplinePoint(FVector(0.f, 200.f, 0.f), ESplineCoordinateSpace::Local, true); // Y축으로 가지
	Spline->UpdateSpline();

	USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);
	SplineMesh->SetMobility(EComponentMobility::Movable);
	SplineMesh->SetStaticMesh(BranchMesh);
	SplineMesh->RegisterComponent();
	SplineMesh->AttachToComponent(Spline, FAttachmentTransformRules::KeepRelativeTransform);
	SplineMesh->SetForwardAxis(ESplineMeshAxis::X); // 메시가 Z축 기준일 경우

	FVector Start, StartTangent, End, EndTangent;
	Spline->GetLocalLocationAndTangentAtSplinePoint(0, Start, StartTangent);
	Spline->GetLocalLocationAndTangentAtSplinePoint(1, End, EndTangent);

	UE_LOG(LogTemp, Warning, TEXT("Start: %s, End: %s"), *Start.ToString(), *End.ToString());

	SplineMesh->SetStartAndEnd(Start, StartTangent, End, EndTangent);

	UE_LOG(LogTemp, Warning, TEXT("Start: %s, End: %s"), *Start.ToString(), *End.ToString());
}