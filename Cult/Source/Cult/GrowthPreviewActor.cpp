// Fill out your copyright notice in the Description page of Project Settings.


#include "GrowthPreviewActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

// Sets default values
AGrowthPreviewActor::AGrowthPreviewActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	SetRootComponent(PreviewMesh);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetRenderCustomDepth(true);
	PreviewMesh->SetCustomDepthStencilValue(252);	// ������ ��
}

// Called when the game starts or when spawned
void AGrowthPreviewActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AGrowthPreviewActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// ��ġ���� (Ŀ������
void AGrowthPreviewActor::SetPreviewLocation(const FVector& Location)
{
	SetActorLocation(Location);
}

// �� �� ���ϱ�
void AGrowthPreviewActor::SetPreviewValid(bool bIsValid)
{
	if (bIsValid)
	{
		PreviewMesh->SetMaterial(0, ValidMaterial);
	}
	else
	{
		PreviewMesh->SetMaterial(0, InvalidMaterial);
	}
}