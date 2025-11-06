// Fill out your copyright notice in the Description page of Project Settings.


#include "Altar.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "CultistCharacter.h"
#include "GameFramework/Controller.h"
#include "NiagaraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"

TSet<AActor*> PlayersInAltar;

// 생성자
AAltar::AAltar()
{
	// tick 활성화
	PrimaryActorTick.bCanEverTick = true;

	// 제단 메쉬
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;



	//if (!CollisionComp->OnComponentBeginOverlap.IsBound())
	//{
	//	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AAltar::OnOverlapBegin);
	//	UE_LOG(LogTemp, Warning, TEXT("OnOverlapBegin manually bound!"));
	//}

	// 메쉬 설정
	//static ConstructorHelpers::FObjectFinder<UStaticMesh> AltarMesh(TEXT("StaticMesh'/Game/Cult_Custom/Modeling/altar5.altar5'"));
	//if (AltarMesh.Succeeded())
	//{
	//	MeshComp->SetStaticMesh(AltarMesh.Object);
	//}

	QTEParticleComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("QTEParticleComp"));
	QTEParticleComponent->SetupAttachment(RootComponent);

	NumCultistsInRange = 0;

	bPlayerInRange = false;
}

// Called when the game starts or when spawned
void AAltar::BeginPlay()
{
	Super::BeginPlay();

	// tick 활성화
	SetActorTickEnabled(true);
	
	PlayersInAltar.Empty();


	UStaticMesh* MeshAsset = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("StaticMesh'/Game/Cult_Custom/Modeling/altar5.altar5'")));
	if (MeshAsset)
	{
		MeshComp->SetStaticMesh(MeshAsset);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("StaticLoadObject failed to load StaticMesh!"));
	}

	CollisionComp = Cast<UBoxComponent>(GetDefaultSubobjectByName(TEXT("CollisionComp2")));
	if (CollisionComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("CollisionComp On"));

		CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AAltar::OnOverlapBegin);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Check Collision!"));
	}
	BaseGainRate = 35.0f;

	// 충돌 이벤트 바인드
	if (CollisionComp)
	{
		CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AAltar::OnOverlapBegin);
		CollisionComp->OnComponentEndOverlap.AddDynamic(this, &AAltar::OnOverlapEnd);
	}
	// 나이아가라 초기 비활성화
	if (QTEParticleComponent)
	{
		QTEParticleComponent->Deactivate();
	}

	// 동적 머터리얼
	if (MeshComp->GetMaterial(0))
	{
		AltarMID = MeshComp->CreateDynamicMaterialInstance(0, MeshComp->GetMaterial(0));
	}
}


void AAltar::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	
	ACultistCharacter* Cultist = Cast<ACultistCharacter>(OtherActor);
	if (Cultist)
	{
		if (!PlayersInAltar.Contains(Cultist))
		{
			PlayersInAltar.Add(Cultist);
			NumCultistsInRange = PlayersInAltar.Num();
		}
		Cultist->SetCurrentAltar(this);
		//NumCultistsInRange++;
		UE_LOG(LogTemp, Warning, TEXT("Cultist entered the altar area"));
	}
	if (OtherActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("Overlap with %s"), *OtherActor->GetName());
	}
}

void AAltar::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{

	// 떠날 때
	ACultistCharacter* Cultist = Cast<ACultistCharacter>(OtherActor);
	if (Cultist)
	{
		PlayersInAltar.Remove(Cultist);
		NumCultistsInRange = PlayersInAltar.Num();
		Cultist->SetCurrentAltar(nullptr);
		//NumCultistsInRange = FMath::Max(0, NumCultistsInRange - 1);	// 최소 0

		// QTE중지
		if (Cultist == CurrentPerformingCultist)
		{
			StopRitualQTE(Cultist);
		}

		UE_LOG(LogTemp, Warning, TEXT("Cultist left. Total : %d"), NumCultistsInRange);
	}
}

// Called every frame
void AAltar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 게이지 자동 충전
	if(NumCultistsInRange > 0)
	{
		// QTE 활성화되지 않았을 때 or 항상 충전
		
		// 항상 충전
		AddToRitualGauge(SlowAutoChargeRate * NumCultistsInRange * DeltaTime);
	}

	// QTE 회전
	if (bIsQTEActive)
	{
		// 현 각도 업데이트(0~360)
		QTECurrentAngle = FMath::Fmod(QTECurrentAngle + (QTERotationSpeed * DeltaTime), 360.0f);

		// 값 정규화
		float NormalizedAngle = QTECurrentAngle / 360.0f;

		UE_LOG(LogTemp, Warning, TEXT("Sending Angle to Niagara: %f"), NormalizedAngle);

		// 파티클 파라미터 업데이트
		if (QTEParticleComponent)
		{
			QTEParticleComponent->SetFloatParameter(FName("User_CurrentAngleNormalized"), NormalizedAngle);
		}
	}
}


void AAltar::IncreaseRitualGauge()
{
	if (NumCultistsInRange > 0)
	{
		float GainRate = BaseGainRate * NumCultistsInRange;
		RitualGauge += GainRate;
		RitualGauge = FMath::Clamp(RitualGauge, 0.0f, 100.0f);
		
		UE_LOG(LogTemp, Warning, TEXT("Ritual Progress: %f"), RitualGauge);

		// 게임모드에서 100 넘었는지 체크하고, 넘었으면 레벨 재시작 
		ACultGameMode* GameMode = Cast<ACultGameMode>(GetWorld()->GetAuthGameMode());
		if (GameMode)
		{
			GameMode->CheckRitualComlete(RitualGauge);
		}
	}
}

void AAltar::AddToRitualGauge(float Amount)
{
	RitualGauge += Amount;
	RitualGauge = FMath::Clamp(RitualGauge, 0.0f, 100.f);

	// 시각효과 업데이트
	if (AltarMID)
	{
		AltarMID->SetScalarParameterValue(FName("Progress"), RitualGauge / 100.0f);
	}
	if (QTEParticleComponent)
	{
		// ProgressColor 파라미터로
		// FLinearColor NewColor = ( 게이지 따라 색상 계산
		// QTEParticleComponent->SetColorParameter(FName("ProgressColor"), NewColor);
	}
	if (RitualGauge >= 100.0f)
	{
		CheckRitualComplete();
	}
}

void AAltar::CheckRitualComplete()
{
	ACultGameMode* GameMode = Cast<ACultGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode)
	{
		GameMode->CheckRitualComlete(RitualGauge);
	}
}

// 의식 시작 시 호출

void AAltar::StartRitualQTE(ACultistCharacter* PerformingCultist)
{
	// 이미 QTE중이면 return
	if (CurrentPerformingCultist != nullptr)return;

	CurrentPerformingCultist = PerformingCultist;

	// 다음 QTE타이머 시작
	GetWorld()->GetTimerManager().SetTimer(QTETriggerTimerHandle, this, &AAltar::TriggerNextQTE, QTEInterval, false);
}

void AAltar::StopRitualQTE(ACultistCharacter* PerformingCultist)
{
	// 의식 중단 시 QTE수행중이던 신도인지
	if (PerformingCultist != CurrentPerformingCultist) return;

	CurrentPerformingCultist = nullptr;
	bIsQTEActive = false;

	if (QTEParticleComponent)
	{
		QTEParticleComponent->Deactivate();
	}
	// 타이머 정지
	GetWorld()->GetTimerManager().ClearTimer(QTETriggerTimerHandle);
}

// 다음QTE활성화
void AAltar::TriggerNextQTE()
{
	// QTE활성화 직전 나갔다면
	if (CurrentPerformingCultist == nullptr)return;

	bIsQTEActive = true;
	QTECurrentAngle = 0.0f;	// 각도 초기화

	//QTE 속도 조절
	QTERotationSpeed = FMath::Lerp(90.0f, 120.0f, RitualGauge / 100.0f);
	if (QTEParticleComponent)
	{
		
		// 성공영역 파라미터 설정
		

		QTEParticleComponent->Activate(true);
	}
}

// Cultist가 입력 시 호출
void AAltar::OnPlayerInput()
{
	// QTE활성화 상태일 때만
	if (!bIsQTEActive || !CurrentPerformingCultist)return;

	// 성공영역의 시작 각도 0~1.0
	float ZoneStartNormalized = QTECurrentAngle / 360.0f;

	// 성공영역 너비 (0.1)
	float ZoneWidthNormalized = 0.1f;

	// 정면 계산	( 카메라방향, 컨트롤러의 회전값)
	AController* PlayerController = CurrentPerformingCultist->GetController();
	if (!PlayerController)return;

	FVector PlayerFacingVector = PlayerController->GetControlRotation().Vector().GetSafeNormal();
	float PlayerFacingDegrees = FMath::RadiansToDegrees(FMath::Atan2(PlayerFacingVector.Y, PlayerFacingVector.X));
	if (PlayerFacingDegrees < 0.0f) { PlayerFacingDegrees += 360.0f; }
	float PlayerFacingNormalized = PlayerFacingDegrees / 360.0f;

	// 판정 - 입력 시 정면에 회전하는 성공영역이 있는지
	float Delta = FMath::Fmod((PlayerFacingNormalized - ZoneStartNormalized) + 1.0f, 1.0f);
	bool bSuccess = (Delta >= 0.0f && Delta <= ZoneWidthNormalized);


	if (bSuccess)
	{
		AddToRitualGauge(QTEBonus);
		UE_LOG(LogTemp, Warning, TEXT("QTE Success"));
		// 성공 파티클 스폰
	}
	else
	{
		AddToRitualGauge(-QTEPenalty);
		UE_LOG(LogTemp, Warning, TEXT("QTE Success"));

		// 실패 파티클
	}

	// QTE 1회 처리 완료
	bIsQTEActive = false;
	if (QTEParticleComponent)
	{
		QTEParticleComponent->Deactivate();
	}
	// 다음 QTE 타이머
	if (CurrentPerformingCultist != nullptr)
	{
		GetWorld()->GetTimerManager().SetTimer(
			QTETriggerTimerHandle, this, &AAltar::TriggerNextQTE, QTEInterval, false
		);
	}
}