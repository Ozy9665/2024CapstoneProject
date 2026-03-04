#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Chaos/ChaosGameplayEventDispatcher.h"
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

	// 캐시 - 태그 기반 수정
	UPROPERTY()
	TArray<TWeakObjectPtr<UGeometryCollectionComponent>> GCWalls;

	UPROPERTY()
	TArray<TWeakObjectPtr<UGeometryCollectionComponent>> GCColumns;

	UPROPERTY()
	TArray<TWeakObjectPtr<UGeometryCollectionComponent>> GCSlabs;



	UFUNCTION(BlueprintCallable, Category = "StructGraph|GC")
	void BuildGCCache();   // 1회 수집

	UFUNCTION(BlueprintCallable, Category = "StructGraph|GC")
	void ClearGCCache();

	UGeometryCollectionComponent* PickRandomValidGC(
		TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr,
		FRandomStream& Stream) const;

	void ForEachValidGC(
		const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr,
		TFunctionRef<void(UGeometryCollectionComponent*)> Fn) const;

	void EnablePhysicsForGCArray(
		const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr,
		bool bEnableSim, bool bEnableGrav);

	FRandomStream Stage2Stream;
	int32 Stage2Seed = 1337; // 나중에 서버에서 받는 값으로 교체

	// 기둥 파괴
	UGeometryCollectionComponent* PickLowestColumnGC() const;

	FTimerHandle Stage3SlabDelayHandle;

	// 슬래브 픽
	UGeometryCollectionComponent* FindNearestSlabGC(const FVector& WorldPoint) const;

	int32 Stage3WallPtIdx = 0;

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

	void SetGCStaticBlocking(FName Tag);

	// 임시 콜리전 조절
	UFUNCTION(BlueprintCallable, Category = "StructGraph")
	void StabilizeStructureComponent(UPrimitiveComponent* PC);

	void EnablePhysicsForTaggedGC(FName Tag, bool bEnableSim, bool bEnableGrav);

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

	FTimerHandle Stage3ContinuousHandle;

	bool bStage3ContinuousRunning = false;
	float Stage3StartTimeSec = 0.f;

	int32 Stage3TickCounter = 0;

	TWeakObjectPtr<UGeometryCollectionComponent> Stage3_WeakColumnGC;
	TWeakObjectPtr<UGeometryCollectionComponent> Stage3_TargetSlabGC;
	FVector Stage3_TargetSlabPunchPoint = FVector::ZeroVector;

	FRandomStream Stage3Stream;

	// ---- Tuning Params (C++ only for now; can be UPROPERTY later) ----
	float Stage3_TotalDuration = 5.0f;
	float Stage3_TickInterval = 0.05f;

	float Stage3_ShakeImpulse = 10.f;
	float Stage3_ShakeUpImpulse = 0.f;

	float Stage3_BaseStrainRadius = 120.f;
	float Stage3_BaseStrainMag = 4000.f;

	float Stage3_WeakStrainRadius = 90.f;
	float Stage3_WeakStrainMag = 120.f;

	float Stage3_PunchRadius = 140.f;
	float Stage3_PunchMag = 90.f;

	float Stage3_GravityRampEndTime = 3.5f;  // 2.5~5.0 (늘릴수록 천천히 처짐)
	bool  bStage3GravityCommitted = false;   // 램프 끝나고 1회만 진짜 gravity ON

	// "Hold" (slab stays responsive but doesn't instantly drop)
	float Stage3_HoldEndTime = 3.5f;  // 2.5~4.5
	float Stage3_SlabHoldLinStart = 15.0f;  // 6~12
	float Stage3_SlabHoldAngStart = 12.0f;  // 4~10
	float Stage3_SlabHoldLinEnd = 1.2f;  // 0.8~2.0
	float Stage3_SlabHoldAngEnd = 1.0f;  // 0.6~2.0
	void EnablePhysicsForGCArray_NoRecreate(
		const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr,
		bool bEnableSim, bool bEnableGrav);

	// ---- Stage3 Continuous API ----
	void StartStage3Continuous();
	void StopStage3Continuous();
	void Stage3_ContinuousTick();

	void EnsureGCPhysicsReady_Stage3();
	void ApplyContinuousShakeToGC(const TArray<TWeakObjectPtr<UGeometryCollectionComponent>>& Arr, float ImpulseStrength);

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
	FName GCWallTag = "GC_WALL";

	UPROPERTY(EditAnywhere, Category = "StructGraph|Stage2|Strain")
	float Stage2_Str_Radius = 180.f;   // 120~300

	UPROPERTY(EditAnywhere, Category = "StructGraph|Stage2|Strain")
	float Stage2_Str_Magnitude = 4000.f; 

	UPROPERTY(EditAnywhere, Category = "StructGraph|Stage2")
	float Stage2_PulseInterval = 0.5f; // 0.15~0.25

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

	UPROPERTY(EditAnywhere, Category = "StructGraph|Stage2")
	float Stage2_KickStrength = 200000.f;

	float Stage3_SlabRampStartTime = 1.2f; 
	float Stage3_SlabRampEndTime = 4.3f;   
	float Stage3_SlabForceMax = 900000.f; 

	int32 Stage3_LowColumnCandidateCount = 6;   // 최저 Z 후보 몇 개 볼지 (4~10)
	float Stage3_PerimeterPreferPower = 1.0f;  // 1.0 기본, 2.0이면 외곽 더 강하게 선호

	UFUNCTION()
	void ApplySlabRampForce_BottomUp(float T);

	UFUNCTION()
	void TriggerPulse2();

	// GC 찾기 + 데미지 적용
	UGeometryCollectionComponent* FindNearestGC(const FVector& WorldPoint) const;
	static void ApplyStrainToGC(
		UGeometryCollectionComponent* GCComp,
		const FVector& HitPoint,
		float Radius,
		float StrainMagnitude,
		int32 Iterations);

	// 스트레인 디버깅
	void Debug_ApplyStrainOnce();

	// 지진
	// 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage1 Debug")
	bool bStage1_InstantCollapse = false;



	////

	UFUNCTION()
	void OnGCBreak(const FChaosBreakEvent& BreakEvent);

	// 중복 바인딩 방지용(선택)
	bool bBoundBreakEvents = false;

	// 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage1 Debug")
	FName Stage1_DestroyTag = "GC_WALL";
	// 카메라
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage1 Debug")
	TSubclassOf<UCameraShakeBase> Stage1_CameraShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Camera")
	TSubclassOf<UCameraShakeBase> QuakeContinuousShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Camera")
	TSubclassOf<UCameraShakeBase> QuakeImpactShakeClass;

	float LastStage2ShakeTime = -1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Camera")
	float Stage1ShakeScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Camera")
	float Stage2ShakeScale = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Camera")
	float Stage3ShakeScale = 2.2f;

	// 카메라 임팩트

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Camera")
	TSubclassOf<UCameraShakeBase> QuakeStage3LongShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Camera")
	float Stage3LongScale = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Camera")
	float Stage3ImpactScale = 1.6f;

	FTimerHandle ImpactDecayTimer;
	float ImpactDecayScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Camera")
	float ImpactStartScale = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Camera")
	float ImpactEndScale = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Camera")
	float ImpactDecayDuration = 1.2f; // 감쇠 총 시간

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Camera")
	float ImpactDecayInterval = 0.15f; // 재호출 간격
	
	void PlayImpactDecaying();
	void PlayShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Stage1 Debug")
	float Stage1_ShakeScale = 2.0f;
	void PlayStage1CameraShake();
	void DestroyActorsWithTag(FName Tag);

	// Stage3 카메라 지속 흔들림 타이머
	FTimerHandle Stage3ShakeTimer;

	// Stage3 흔들림 반복 간격
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Camera")
	float Stage3ShakeInterval = 0.5f;

	// ===== Damping Control =====

	// 초기 정착 구간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Damping")
	float SettleDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Damping")
	float SettleLinearDamping = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Damping")
	float SettleAngularDamping = 15.0f;

	// Stage별 댐핑
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Damping")
	float Stage1LinearDamping = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Damping")
	float Stage1AngularDamping = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Damping")
	float Stage2LinearDamping = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Damping")
	float Stage2AngularDamping = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Damping")
	float Stage3LinearDamping = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake|Damping")
	float Stage3AngularDamping = 0.2f;

	void ApplyDampingToTaggedGC(FName Tag, float Lin, float Ang);
	void ApplySettleDampingThenRestore();

	FTimerHandle SettleTimerHandle;

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
	int32 Stage2_LocalDamageCount = 1; // 벽 1~2개 정도

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
	bool bDrawDebug = false;

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
