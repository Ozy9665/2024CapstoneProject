// Fill out your copyright notice in the Description page of Project Settings.

#include "MySocketActor.h"

// Sets default values
AMySocketActor::AMySocketActor()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void AMySocketActor::BeginPlay()
{
    Super::BeginPlay();
}

FVector AMySocketActor::ToUE(const FNetVec& v)
{
	return FVector{
        static_cast<double>(v.x),
        static_cast<double>(v.y),
        static_cast<double>(v.z)
    };
}

FRotator AMySocketActor::ToUE(const FNetRot& r)
{
	return FRotator(
        static_cast<double>(r.x),
        static_cast<double>(r.y),
        static_cast<double>(r.z)
    );
}

FNetVec AMySocketActor::ToNet(const FVector& v)
{
    return FNetVec{
        static_cast<double>(v.X),
        static_cast<double>(v.Y),
        static_cast<double>(v.Z)
    };
}

FNetRot AMySocketActor::ToNet(const FRotator& r)
{
    return FNetRot{
        static_cast<double>(r.Pitch),
        static_cast<double>(r.Yaw),
        static_cast<double>(r.Roll)
    };
}

// Called every frame
void AMySocketActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}