// Fill out your copyright notice in the Description page of Project Settings.


#include "BuildingBlockComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

// Sets default values for this component's properties
UBuildingBlockComponent::UBuildingBlockComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...

	SetSimulatePhysics(false);
	SetNotifyRigidBodyCollision(true);
}


// Called when the game starts
void UBuildingBlockComponent::BeginPlay()
{
	Super::BeginPlay();


}

// Called every frame
void UBuildingBlockComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UBuildingBlockComponent::ReceiveImpulse(float Impulse, FVector Direction)
{
	if (bCollapsed) return;
	
	CurrentImpulse += Impulse;

	if (CurrentImpulse >= MaxImpulseThreshold)
	{
		// �ر��� ���ÿ� ���޵� �ؾ�����?
		//PropagateImpulse(Impulse * AttenuationCoefficient, Direction);

		Collapse();
	}
	else
	{
		PropagateImpulse(Impulse * AttenuationCoefficient, Direction);
	}
}

void UBuildingBlockComponent::PropagateImpulse(float Impulse, FVector Direction)
{
	for (UBuildingBlockComponent* Neighbor : ConnectedBlocks)
	{
		if (Neighbor && !Neighbor->bCollapsed)
		{
			float Delay = FMath::FRandRange(0.1f, 0.3f);
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, [Neighbor, Impulse, Direction]()
				{
					Neighbor->ReceiveImpulse(Impulse, Direction);
				}, Delay, false);
		}
	}
}

void UBuildingBlockComponent::Collapse()
{
	if (bCollapsed) return;

	bCollapsed = true;

	UStaticMeshComponent* MeshComp = GetOwner()->FindComponentByClass<UStaticMeshComponent>();
	if (MeshComp)
	{
		MeshComp->SetMobility(EComponentMobility::Movable);
		MeshComp->SetSimulatePhysics(true);
		MeshComp->SetEnableGravity(true); // Ȥ�ö� �������� ��� ���
		MeshComp->WakeAllRigidBodies();   // ���� ���� ��� ��� ����
		MeshComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Collapse: No StaticMeshComponent found on %s"), *GetOwner()->GetName());
	}


	for (UBuildingBlockComponent* Neighbor : ConnectedBlocks)
	{
		if (Neighbor && !Neighbor->bCollapsed)
		{
			float TransferImpulse = Mass * 300.0f;
			Neighbor->ReceiveImpulse(TransferImpulse, FVector::UpVector);
		}
	}
}