// Fill out your copyright notice in the Description page of Project Settings.

#include "ClientCharacter.h"
#include "Components/SkeletalMeshComponent.h"

// Sets default values
AClientCharacter::AClientCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

    if (GetMesh())
    {
        GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f)); // �޽� ��ġ ����
        GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f)); // �޽� ȸ�� ����
    }
}

// Called when the game starts or when spawned
void AClientCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AClientCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
