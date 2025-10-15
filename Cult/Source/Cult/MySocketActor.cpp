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
	return FVector(v.x, v.y, v.z);
}

FRotator AMySocketActor::ToUE(const FNetRot& r)
{
	return FRotator(r.x, r.y, r.z);
}

FNetVec AMySocketActor::ToNet(const FVector& v)
{
    FNetVec out;
    out.x = static_cast<double>(v.X);
    out.y = static_cast<double>(v.Y);
    out.z = static_cast<double>(v.Z);
    return out;
}

FNetRot AMySocketActor::ToNet(const FRotator& r)
{
    FNetRot out;
    out.x = static_cast<double>(r.Pitch);
    out.y = static_cast<double>(r.Yaw);
    out.z = static_cast<double>(r.Roll);
    return out;
}

// Called every frame
void AMySocketActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}