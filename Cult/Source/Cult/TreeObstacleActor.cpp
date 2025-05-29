// Safe and refined version of ATreeObstacleActor branch generation and animation

#include "TreeObstacleActor.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/World.h"

ATreeObstacleActor::ATreeObstacleActor()
{
	PrimaryActorTick.bCanEverTick = true;

	TreeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TreeMesh"));
	RootComponent = TreeMesh;

	TreeMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TreeMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TreeMesh->SetSimulatePhysics(false);

	GrowState = ETreeGrowState::GrowingTrunk;
}

void ATreeObstacleActor::BeginPlay()
{
	Super::BeginPlay();
	InitialLocation = GetActorLocation();
	SetActorLocation(InitialLocation - FVector(0, 0, GrowHeight));
}

void ATreeObstacleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GrowState == ETreeGrowState::GrowingTrunk)
	{	
		ElapsedTime += DeltaTime;
		float Alpha = FMath::Clamp(ElapsedTime / GrowTime, 0.f, 1.f);

		// Z스케일
		FVector NewScale = FMath::Lerp(FVector(0.01f, 0.01f, 0.01f), FVector(1.f, 1.f, 1.f), Alpha);
		TreeMesh->SetWorldScale3D(NewScale * 0.1f) ;

		// 위로만 자라도록 
		FVector BaseLocation = InitialLocation;
		FVector ScaleOffset = FVector(0.f, 0.f, GrowHeight * (0.5f * (1 - NewScale.Z)));  // 중간점 보정
		SetActorLocation(BaseLocation - ScaleOffset);

		if (Alpha >= 1.f)
		{
			GrowState = ETreeGrowState::GrowingBranches;
			ElapsedTime = 0.f;
			SetupBranches();
		}
	}
	else if (GrowState == ETreeGrowState::GrowingBranches)
	{
		bool bAllGrown = true;

		for (FBranchData& Branch : AllBranches)
		{
			if (!Branch.Spline || Branch.Meshes.Num() == 0) continue;

			float TotalLength = Branch.Spline->GetSplineLength();
			Branch.CurrentLength += BranchGrowSpeed * DeltaTime;

			if (Branch.CurrentLength < TotalLength)
				bAllGrown = false;

			const int32 NumSegments = Branch.Meshes.Num();
			for (int32 j = 0; j < NumSegments; ++j)
			{
				USplineMeshComponent* Segment = Branch.Meshes[j];
				if (!Segment) continue;

				const float SegmentStart = TotalLength / NumSegments * j;
				const float SegmentEnd = TotalLength / NumSegments * (j + 1);

				if (Branch.CurrentLength < SegmentStart) continue;

				float VisibleLength = FMath::Clamp(Branch.CurrentLength - SegmentStart, 0.f, SegmentEnd - SegmentStart);

				FVector Start = Branch.Spline->GetLocationAtDistanceAlongSpline(SegmentStart, ESplineCoordinateSpace::Local);
				FVector End = Branch.Spline->GetLocationAtDistanceAlongSpline(SegmentStart + VisibleLength, ESplineCoordinateSpace::Local);
				FVector StartTangent = Branch.Spline->GetTangentAtDistanceAlongSpline(SegmentStart, ESplineCoordinateSpace::Local) * 1.f;
				FVector EndTangent = Branch.Spline->GetTangentAtDistanceAlongSpline(SegmentStart + VisibleLength, ESplineCoordinateSpace::Local) * 1.f;

				Segment->SetVisibility(true);
				Segment->SetStartAndEnd(Start, StartTangent, End, EndTangent);
			}
		}

		if (bAllGrown)
		{
			GrowState = ETreeGrowState::Done;
			SetActorTickEnabled(false);
			UE_LOG(LogTemp, Warning, TEXT("🌲 전체 가지 성장 완료"));
		}
	}
	else if (GrowState == ETreeGrowState::Done)
	{
		SetActorTickEnabled(false); // 혹시 다시 켜진 경우 방지
	}
}

void ATreeObstacleActor::SetupBranches()
{
	UE_LOG(LogTemp, Warning, TEXT("SetupBranches"));
	AllBranches.Empty();

	const int32 NumFirstBranches = 12;
	const float MinHeightOffset = 100.f; // 트렁크 중간 이상
	const float MaxHeightOffset = GrowHeight - 30.f;

	for (int32 i = 0; i < NumFirstBranches; ++i)
	{
		float HeightOffset = FMath::FRandRange(MinHeightOffset, MaxHeightOffset);
		FVector Start = FVector(InitialLocation.X, InitialLocation.Y, InitialLocation.Z + HeightOffset);

		float Angle = FMath::FRandRange(0.f, 360.f);
		FRotator Rotation = FRotator(0.f, Angle, 0.f);

		CreateBranch(Start, Rotation, 0);
	}
}

void ATreeObstacleActor::CreateBranch(const FVector& StartLocation, const FRotator& Rotation, int32 Depth)
{
	if (Depth > MaxBranchDepth) return;

	FBranchData Branch;
	Branch.Depth = Depth;
	Branch.InitialDirection = Rotation.RotateVector(FVector(1, 0, 0));

	// Spline 생성
	USplineComponent* Spline = NewObject<USplineComponent>(this);
	Spline->SetMobility(EComponentMobility::Movable);
	Spline->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	Spline->SetWorldLocation(StartLocation);
	Spline->SetWorldRotation(Rotation);
	Spline->RegisterComponent();
	AddInstanceComponent(Spline);
	Branch.Spline = Spline;


	const int32 NumSegments = 6;
	for (int32 i = 0; i <= NumSegments; ++i)
	{
		float T = static_cast<float>(i) / NumSegments;
		float HeightFactor = FMath::Clamp((StartLocation.Z - InitialLocation.Z) / GrowHeight, 0.f, 1.f);
		float AdjustedLength = FMath::Lerp(BranchMaxLength, BranchMaxLength * 0.4f, HeightFactor);
		FVector Base = FVector(T * AdjustedLength, 0.f, 0.f);

		//FVector Offset = FVector(0, FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-10.f, 40.f));
		FVector Offset = FVector::ZeroVector;
		
		FVector WorldCheckPos = Spline->GetComponentTransform().TransformPosition(Base + Offset);
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			Hit,
			WorldCheckPos,
			WorldCheckPos + FVector(0, 0, -20.f),
			ECC_WorldStatic,
			Params
		);

		if (bBlocked)
		{
			UE_LOG(LogTemp, Warning, TEXT("Branch growth blocked by something"));
			break; // 가지 멈춤
		}

		if (i != 0 && i != NumSegments)
		{
			float CurveAmount = FMath::Sin(T * PI) * 30.f; // 중앙에서 최대 휘어짐
			float RandZ = CurveAmount + FMath::FRandRange(-10.f, 10.f);
			float RandY = FMath::FRandRange(-10.f, 10.f);
			Offset = FVector(0.f, RandY, RandZ);
		}
		Spline->AddSplinePoint(Base + Offset, ESplineCoordinateSpace::Local);
		Spline->SetSplinePointType(i, ESplinePointType::Curve);
	}
	Spline->UpdateSpline();

	for (int32 i = 0; i < NumSegments; ++i)
	{
		USplineMeshComponent* Mesh = NewObject<USplineMeshComponent>(this);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(BranchMesh);
		Mesh->SetForwardAxis(ESplineMeshAxis::X);

		float TaperStart = FMath::Lerp(1.5f, 0.1f, (float)i / NumSegments);
		float TaperEnd = FMath::Lerp(1.5f, 0.1f, (float)(i + 1) / NumSegments);
		Mesh->SetStartScale(FVector2D(TaperStart, TaperStart));
		Mesh->SetEndScale(FVector2D(TaperEnd, TaperEnd));

		Mesh->AttachToComponent(Spline, FAttachmentTransformRules::KeepRelativeTransform);
		Mesh->SetVisibility(false);
		Mesh->RegisterComponent();
		AddInstanceComponent(Mesh);

		Branch.Meshes.Add(Mesh);
	}
	Branch.CurrentLength = 0.f;
	AllBranches.Add(Branch);

	// 재귀적으로 가지치기
	if (Depth + 1 <= MaxBranchDepth)
	{
		const int32 NumChildren = FMath::RandRange(1, 2);
		for (int32 k = 0; k < NumChildren; ++k)
		{
			FVector ParentDir = Rotation.RotateVector(FVector::ForwardVector);
			float Angle = FMath::FRandRange(-40.f, 40.f);
			FVector NewDir = ParentDir.RotateAngleAxis(Angle, FVector::UpVector);
			NewDir = (NewDir + FVector::UpVector * 0.4f).GetSafeNormal(); // 위로 기울어짐
			FRotator ChildRotation = NewDir.Rotation();
			FVector EndWorld = Spline->GetLocationAtSplinePoint(NumSegments, ESplineCoordinateSpace::World);
			float UpDot = FVector::DotProduct(NewDir.GetSafeNormal(), FVector::UpVector);
			if (UpDot > 0.6f) continue;
			CreateBranch(EndWorld, ChildRotation, Depth + 1);
		}
	}
}