// Fill out your copyright notice in the Description page of Project Settings.


#include "GrowthPreviewActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

// Sets default values
AGrowthPreviewActor::AGrowthPreviewActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	RootComponent = PreviewMesh;

	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetRenderCustomDepth(true);	// 실루엣
	PreviewMesh->SetCustomDepthStencilValue(1);	
}

// Called when the game starts or when spawned
void AGrowthPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	if (PreviewMesh && PreviewMesh->GetMaterial(0))
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(PreviewMesh->GetMaterial(0), this);
		PreviewMesh->SetMaterial(0, DynamicMaterial);
	}
}

// Called every frame
void AGrowthPreviewActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// 위치지정 (커서기준
void AGrowthPreviewActor::UpdatePreviewLocation(const FVector& NewLocation)
{
	SetActorLocation(NewLocation);
}

// 초 파 정하기
void AGrowthPreviewActor::SetValidPlacement(bool bIsValid)
{
	bIsPlacementValid = bIsValid;

	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), bIsValid ? ValidColor : InvalidColor);
	}
}