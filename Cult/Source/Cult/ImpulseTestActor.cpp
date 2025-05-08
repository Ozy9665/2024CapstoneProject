//// Fill out your copyright notice in the Description page of Project Settings.
//
//
//#include "ImpulseTestActor.h"
//#include "BuildingBlockComponent.h"
//
//// Sets default values
//AImpulseTestActor::AImpulseTestActor()
//{
// 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
//	PrimaryActorTick.bCanEverTick = false;
//
//}
//
//// Called when the game starts or when spawned
//void AImpulseTestActor::BeginPlay()
//{
//	Super::BeginPlay();
//	if (TargetActor)
//	{
//		if (UBuildingBlockComponent* BlockComp = TargetActor->FindComponentByClass<UBuildingBlockComponent>())
//		{
//			BlockComp->ApplyImpulse(Impulse);
//		}
//	}
//}
//
//// Called every frame
//void AImpulseTestActor::Tick(float DeltaTime)
//{
//	Super::Tick(DeltaTime);
//
//}
//
