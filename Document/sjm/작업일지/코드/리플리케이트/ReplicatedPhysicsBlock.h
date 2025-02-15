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

    // ���ø����̼��� ���� ����
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ����� �޽� (�浹 & ���� ó��)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
        class UStaticMeshComponent* MeshComp;

    // �������� ����ȭ�� ��ġ
    UPROPERTY(ReplicatedUsing = OnRep_UpdatePhysics, VisibleAnywhere, Category = "Replication")
        FVector ReplicatedLocation;

    // �������� ����ȭ�� ȸ����
    UPROPERTY(ReplicatedUsing = OnRep_UpdatePhysics, VisibleAnywhere, Category = "Replication")
        FRotator ReplicatedRotation;

    // �������� ��ġ & ȸ������ ������Ʈ�ϴ� �Լ�
    UFUNCTION(Server, Reliable, WithValidation)
        void Server_UpdatePhysics(FVector NewLocation, FRotator NewRotation);
    void Server_UpdatePhysics_Implementation(FVector NewLocation, FRotator NewRotation);
    bool Server_UpdatePhysics_Validate(FVector NewLocation, FRotator NewRotation);

    // Ŭ���̾�Ʈ���� ��ġ�� �����ϴ� �Լ�
    UFUNCTION()
        void OnRep_UpdatePhysics();
};
