// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ReplicatedPhysicsBlock.generated.h"

UCLASS()
class PROJECT2_API AReplicatedPhysicsBlock : public AActor
{
	GENERATED_BODY()

public:
    AReplicatedPhysicsBlock();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // 리플리케이션을 위한 설정
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 블록의 메쉬 (충돌 & 물리 처리)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
        class UStaticMeshComponent* MeshComp;

    // 서버에서 동기화할 위치
    UPROPERTY(ReplicatedUsing = OnRep_UpdatePhysics, VisibleAnywhere, Category = "Replication")
        FVector ReplicatedLocation;

    // 서버에서 동기화할 회전값
    UPROPERTY(ReplicatedUsing = OnRep_UpdatePhysics, VisibleAnywhere, Category = "Replication")
        FRotator ReplicatedRotation;

    // 서버에서 위치 & 회전값을 업데이트하는 함수
    UFUNCTION(Server, Reliable, WithValidation)
        void Server_UpdatePhysics(FVector NewLocation, FRotator NewRotation);
    void Server_UpdatePhysics_Implementation(FVector NewLocation, FRotator NewRotation);
    bool Server_UpdatePhysics_Validate(FVector NewLocation, FRotator NewRotation);

    // 클라이언트에서 위치를 보정하는 함수
    UFUNCTION()
        void OnRep_UpdatePhysics();
};
