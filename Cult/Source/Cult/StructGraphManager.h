#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "StructGraphManager.generated.h"

UENUM(BlueprintType)
enum class EQuakeStage : uint8
{
	None,
	Stage1,
	Stage2,
	Stage3
};

UENUM(BlueprintType)
enum class EStructNodeType : uint8
{
	Slab,
	Column,
	Wall
};

UENUM(BlueprintType)
enum class EStructDamageState : uint8
{
	Intact,
	Yield,
	Failed
};

UENUM(BlueprintType)
enum class EQuakePhase : uint8
{
	Idle,
	Micro,
	Progress,
	Collapse
};

// 그래프 노드
USTRUCT(BlueprintType)
struct FStructGraphNode
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 Id = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EStructNodeType Type = EStructNodeType::Slab;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EStructDamageState State = EStructDamageState::Intact;

	// 층
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 FloorIndex = -1;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Comp = nullptr;

	// 하중/용량
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float SelfWeight = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CarriedLoad = 0.f; // 위에서 내려온 누적 하중(재분배 후 값)

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Capacity = 1.f; // >0

	// 슬래브가 지지받는 대상(기둥/벽) 노드 Id들
	UPROPERTY()
	TArray<int32> Supports;

	UPROPERTY()
	TArray<float> SupportWeights;

	UPROPERTY()
	TArray<int32> DownSlabs;

	UPROPERTY()
	TArray<float> DownWeights;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float BaseCapacity = 0.f;   // 초기화 이후 용량

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Damage = 0.f;         // 누적 손상(0~1)

	// 연출1~3
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float DamageAccum = 0.f;   // 0~무한 (누적)

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Crack01 = 0.f;       // 0~1 (Stage1)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Spall01 = 0.f;       // 0~1 (Stage2 박리 레벨)


	// 계산용
	float Utilization() const { return (Capacity <= KINDA_SMALL_NUMBER) ? 999.f : ((SelfWeight + CarriedLoad) / Capacity); }

	void ClearSupports()
	{
		Supports.Reset();
		SupportWeights.Reset();
	}

	void ClearDown()
	{
		DownSlabs.Reset();
		DownWeights.Reset();
	}
};




// 클래스 
UCLASS()
class CULT_API AStructGraphManager : public AActor
{
	GENERATED_BODY()

public:
	AStructGraphManager();


	// 수집한 컴포넌트 배열 받고 그래프 구성 + 초기 계산
	UFUNCTION(BlueprintCallable, Category = "StructGraph")
	void InitializeFromBP(
		const TArray<UStaticMeshComponent*>& InSlabL1,
		const TArray<UStaticMeshComponent*>& InSlabL2,
		const TArray<UStaticMeshComponent*>& InSlabL3,
		const TArray<UStaticMeshComponent*>& InSlabRoof,
		const TArray<UStaticMeshComponent*>& InColumns,
		const TArray<UStaticMeshComponent*>& InWalls
	);

	// 임시 콜리전 조절
	UFUNCTION(BlueprintCallable, Category = "StructGraph")
	void StabilizeStructureComponent(UPrimitiveComponent* PC);

	// 지진
	UFUNCTION(BlueprintCallable, Category = "StructGraph")
	void StartEarthquake();

	UFUNCTION(BlueprintCallable, Category = "StructGraph")
	void StopEarthquake();

	UFUNCTION(BlueprintCallable, Category = "StructGraph")
	void ResetStructure();

	UFUNCTION(BlueprintCallable, Category = "Quake")
	void SetQuakeStage(EQuakeStage NewStage);

	UFUNCTION(BlueprintCallable, Category = "Quake")
	void TriggerStage1();

	UFUNCTION(BlueprintCallable, Category = "Quake")
	void TriggerStage2();

	UFUNCTION(BlueprintCallable, Category = "Quake")
	void TriggerStage3();

	// 초기 캘리브레이션
	UPROPERTY(EditAnywhere, Category = "StructGraph|Calib")
	float TargetInitialUtilization = 0.35f; // 초기 평균 U 목표

	UPROPERTY(EditAnywhere, Category = "StructGraph|Calib")
	float CapacitySafetyFactor = 2.5f; // 추가 여유

	UFUNCTION(BlueprintCallable, Category = "StructGraph|Calib")
	void CalibrateCapacitiesFromInitialState();

	//
	UFUNCTION(BlueprintNativeEvent, Category = "StructGraph|VFX")
	void OnNodeYield(UPrimitiveComponent* Comp, float Utilization);

	UFUNCTION(BlueprintNativeEvent, Category = "StructGraph|VFX")
	void OnNodeFailed(UPrimitiveComponent* Comp, float Utilization);

	UFUNCTION(BlueprintCallable, Category = "StructGraph|Damage")
	void CacheBaseCapacities();
	UFUNCTION(BlueprintCallable, Category = "StructGraph|Damage")
	void ApplySeismicAndShearDemands(float SeismicFactor);
	
	UFUNCTION(BlueprintCallable, Category = "StructGraph|Damage")
	void AccumulateDamage(float DeltaTime);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake")
	float Phase1_Duration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake")
	float Phase2_Duration = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake")
	float Phase3_Duration = 6.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quake")
	EQuakePhase QuakePhase = EQuakePhase::Idle;

	UFUNCTION(BlueprintCallable, Category = "Quake")
	void StartEarthquake3Phase();

	UFUNCTION(BlueprintCallable, Category = "Quake")
	void StopEarthquake3Phase();

	// Geometry Collection 
	UPROPERTY(EditAnywhere, Category = "StructGraph|Stage2")
	FName GCWallTag = "GCWALL";

	UPROPERTY(EditAnywhere, Category = "StructGraph|Stage2|Strain")
	float Stage2_Str_Radius = 180.f;   // 120~300

	UPROPERTY(EditAnywhere, Category = "StructGraph|Stage2|Strain")
	float Stage2_Str_Magnitude = 45000.f; 

	UPROPERTY(EditAnywhere, Category = "StructGraph|Stage2")
	float Stage2_PulseInterval = 0.2f; // 0.15~0.25

	UPROPERTY(EditAnywhere, Category = "StructGraph|Stage2")
	float Stage2_Duration = 4.0f; // 3~6

	UPROPERTY(EditAnywhere, Category = "StructGraph|Stage2")
	float Stage2_DamageMin = 5000.f;

	UPROPERTY(EditAnywhere, Category = "StructGraph|Stage2")
	float Stage2_DamageMax = 30000.f;

	UPROPERTY(EditAnywhere, Category = "StructGraph|Stage2")
	float Stage2_ImpulseScale = 0.6f;

	FTimerHandle Stage2Timer;
	float Stage2Elapsed = 0.f;

	UFUNCTION()
	void TriggerPulse2();

	// GC 찾기 + 데미지 적용
	UGeometryCollectionComponent* FindNearestGC(const FVector& WorldPoint) const;
	static void ApplyStrainToGC(
		UGeometryCollectionComponent* GCComp,
		const FVector& HitPoint,
		float Radius,
		float StrainMagnitude);

	// 스트레인 디버깅
	void Debug_ApplyStrainOnce();

	// 지진
	// 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage1 Debug")
	bool bStage1_InstantCollapse = true;

	// 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage1 Debug")
	FName Stage1_DestroyTag = "GC_WALL";
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage1 Debug")
	TSubclassOf<UCameraShakeBase> Stage1_CameraShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage1 Debug")
	float Stage1_ShakeScale = 2.0f;
	void PlayStage1CameraShake();
	void DestroyActorsWithTag(FName Tag);


protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake")
	EQuakeStage QuakeStage = EQuakeStage::None;

	// Stage 파라미터 (MVP)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage1")
	float Stage1_SeismicBase = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage1")
	float Stage1_Omega = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage2")
	int32 Stage2_LocalDamageCount = 2; // 벽 1~2개 정도

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage2")
	float Stage2_LocalDamageStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage3")
	float Stage3_SeismicBase = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage3")
	float Stage3_Omega = 8.0f;

	// Stage2에서의 부분 파손 대상 후보?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage2")
	TArray<TObjectPtr<UPrimitiveComponent>> Stage2Targets;



private:
	// ===== 파라미터(튜닝 포인트) =====
	UPROPERTY(EditAnywhere, Category = "StructGraph|Params")
	float SupportZTolerance = 15.f; // 지지로 간주할 값

	UPROPERTY(EditAnywhere, Category = "StructGraph|Params")
	float SupportXYExpand = 10.f;   // 바운드로 겹침 판정 완화

	UPROPERTY(EditAnywhere, Category = "StructGraph|Params")
	float WeightDistEpsilon = 30.f; // cm

	UPROPERTY(EditAnywhere, Category = "StructGraph|Params")
	float WeightDistPower = 2.f; // p=2 or 완만하게 1

	UPROPERTY(EditAnywhere, Category = "StructGraph|Params")
	float WallTypeFactor = 1.15f; // 벽 지지 효율

	UPROPERTY(EditAnywhere, Category = "StructGraph|Params")
	float ColumnTypeFactor = 1.00f;

	UPROPERTY(EditAnywhere, Category = "StructGraph|Params")
	float AnglePower = 2.f; // 기둥 기울면 효율 감소

	UPROPERTY(EditAnywhere, Category = "StructGraph|Params")
	int32 MaxSolveIterations = 6;   // 실패가 연쇄로 이어질 때 수렴 루프

	// 지진 가중  수직하중 -> 연쇄붕괴
	UPROPERTY(EditAnywhere, Category = "StructGraph|Earthquake")
	float SeismicBase = 0.35f;

	UPROPERTY(EditAnywhere, Category = "StructGraph|Earthquake")
	float SeismicOmega = 3.5f; // 진동수(라디안/초)

	UPROPERTY(EditAnywhere, Category = "StructGraph|Earthquake")
	float SeismicFloorAmplify = 0.10f; // 층이 높을수록 흔들림/요구량 증가

	UPROPERTY(EditAnywhere, Category = "StructGraph|Earthquake")
	float SeismicToVerticalScale = 0.35f; // 수평 요구량 -> 수직 등가 스케일

	UPROPERTY(EditAnywhere, Category = "StructGraph|Debug")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, Category = "StructGraph|Debug")
	float DebugLineDuration = 0.f; // 0 - 한 프레임

	UPROPERTY(EditAnywhere, Category = "StructGraph|Shear")
	int32 SoftStoryFloor = 1; // 전단 집중 층 (1층)

	UPROPERTY(EditAnywhere, Category = "StructGraph|Shear")
	float SoftStoryBoost = 2.0f; // 전단 증폭

	UPROPERTY(EditAnywhere, Category = "StructGraph|Shear")
	float ShearToVerticalScale = 0.25f; // 전단 요구량을 등가 수직으로 변환

	UPROPERTY(EditAnywhere, Category = "StructGraph|Damage")
	float DamageRateYield = 0.08f; // 초당 누적 속도

	UPROPERTY(EditAnywhere, Category = "StructGraph|Damage")
	float DamageRateFailed = 0.25f; // 초당 누적 속도(파괴 직전 가속)

	UPROPERTY(EditAnywhere, Category = "StructGraph|Damage")
	float CapacityLossAtFullDamage = 0.65f; // Damage=1 일 때 용량이 65% 감소

	UPROPERTY(EditAnywhere, Category = "StructGraph|Damage")
	float YieldStart = 0.85f;

	UPROPERTY(EditAnywhere, Category = "StructGraph|Damage")
	float FailStart = 1.0f;


	// ===== 내부 데이터 =====
	UPROPERTY()
	TArray<FStructGraphNode> Nodes;

	UPROPERTY()
	TMap<TObjectPtr<UPrimitiveComponent>, FTransform> OriginalTransforms;

	bool bRunning = false;
	float Elapsed = 0.f;
	FTimerHandle TickHandle;

	// BP에서 넘어온 캐시
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SlabL1;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SlabL2;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SlabL3;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SlabRoof;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Columns;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Walls;


private:
	void BuildNodes();
	void SaveOriginalTransforms();
	void BuildSupportGraph();
	void SolveLoadsAndDamage();

	void ComputeLoadsFromScratch(float SeismicFactor);
	bool UpdateDamageStates(bool& bAnyNewFailures);

	int32 AddNode(EStructNodeType Type, UPrimitiveComponent* Comp, int32 FloorIndex);
	FBox GetCompBox(UPrimitiveComponent* Comp) const;
	static bool Overlap2D(const FBox& A, const FBox& B, float Expand);
	static float OverlapArea2D(const FBox& A, const FBox& B, float Expand);

	void DrawDebugGraph();
	void TickEarthquake();

	// Down graph
	void BuildDownGraph();

	// BFS -> 지지대가 Ground까지 연결되는지
	bool IsConnectedToGround_BFS(int32 StartNodeId, TMap<int32, bool>& Cache) const;

	// Box helper
	static float GetTopZ(const FBox& Box) { return Box.Max.Z; }
	static float GetBottomZ(const FBox& Box) { return Box.Min.Z; }
};
