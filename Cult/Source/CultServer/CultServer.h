#pragma once

#include "CoreMinimal.h" // ← 모든 UE 헤더 먼저

#include "CultServer.generated.h" // ← 항상 마지막에 위치해야 함

UCLASS()
class UCultServer : public UObject // 예시
{
    GENERATED_BODY()

public:
    void Run();
};
