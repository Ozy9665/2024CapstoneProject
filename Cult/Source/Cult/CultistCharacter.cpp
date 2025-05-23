// Fill out your copyright notice in the Description page of Project Settings.


#include "CultistCharacter.h"
#include "MySocketCultistActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GrowthPreviewActor.h"
#include "TreeObstacleActor.h"
#include "GameFramework/PlayerController.h"
#include "Components/ProgressBar.h"
#include "Components/InputComponent.h"
#include "Landscape.h"
#include "DrawDebugHelpers.h"

extern AMySocketCultistActor* MySocketCultistActor;

ACultistCharacter::ACultistCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	WalkSpeed = 600.0f;
	

	Health = 100.0f;
	CurrentHealth = 100.0f;
	bIsStunned = false;
	bIsAlreadyStunned = false;
	bIsDead = false;

	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	

	bIsPerformingRitual = false;
	RitualProgress = 0.0f;
	RitualSpeed = 50.0f; // 초당 증가속도

	// 의식 수행 작업 진행도
	TaskRitualProgress = 0.0f;
	TaskRitualSpeed = 20.0f;

	RangeVisualizer = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RangeVisualizer"));
	RangeVisualizer->SetupAttachment(RootComponent);

	RangeVisualizer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RangeVisualizer->SetCastShadow(false);
	RangeVisualizer->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // XY 평면으로 눕힘
	RangeVisualizer->SetRelativeLocation(FVector(0.f, 0.f, 1.f));     // 약간 위에 표시

}

void ACultistCharacter::BeginPlay()
{
	Super::BeginPlay();
	// 초기화 값
	WalkSpeed = 600.0f;
	Health = 100.0f;
	CurrentHealth = 100.0f;
	bIsStunned = false;
	bIsAlreadyStunned = false;
	bIsDead = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	bIsPerformingRitual = false;
	RitualProgress = 0.0f;
	RitualSpeed = 50.0f; // 초당 증가속도
	TaskRitualProgress = 0.0f;	// 의식수행 작업 진행도
	TaskRitualSpeed = 20.0f;

	// 컨트롤러 yaw 회전 false
	bUseControllerRotationYaw = false;
	// 입력 방향으로 회전
	GetCharacterMovement()->bOrientRotationToMovement = true;
	// 회전속도
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	//
	//// 마찰율줄이기
	//GetCharacterMovement()->GroundFriction = 0.1f; 

	GetCharacterMovement()->SetMovementMode(MOVE_Walking);


	SpringArmComp = FindComponentByClass<USpringArmComponent>();
	if (SpringArmComp)
	{
		SpringArmComp->TargetArmLength = 300.0f;
		SpringArmComp->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
		UE_LOG(LogTemp, Warning, TEXT("Set SpringArm"));
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("SpringArm null"));
		SpringArmComp = NewObject<USpringArmComponent>(this);
		if (SpringArmComp)
		{
			SpringArmComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			SpringArmComp->RegisterComponent();
			SpringArmComp->TargetArmLength = 300.0f;
			SpringArmComp->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
			UE_LOG(LogTemp, Warning, TEXT("So make SpringArm"));
		}
		else {
			UE_LOG(LogTemp, Error, TEXT("Still Null SpringArm"));
		}
	}
	if (!TaskRitualWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("TaskRitualWidgetClass is NULL! Make sure it's set in Blueprint."));
	}
	if (TaskRitualWidgetClass)
	{
		TaskRitualWidget = CreateWidget<UUserWidget>(GetWorld(), TaskRitualWidgetClass);
		if (TaskRitualWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("TaskRitualWidget Created"));
			TaskRitualWidget->AddToViewport();
			TaskRitualWidget->SetVisibility(ESlateVisibility::Hidden);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to create widget"));
		}
	}
	// 컨트롤러 확인
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->GetPawn())
	{
		UE_LOG(LogTemp, Warning, TEXT("Possessed pawn: %s"), *PC->GetPawn()->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("NO PlayerController OR NO Pawn"));
	}

	AController* CController = GetController();
	if (CController)
	{
		UE_LOG(LogTemp, Warning, TEXT("Controller: %s"), *CController->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("No Controller found on BeginPlay"));
	}

	if (PC)
	{
		// 마우스 커서 숨기고, 게임 전용 입력 모드로 복원
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}

	if (BloodWidgetClass)
	{
		BloodEffectWidget = CreateWidget<UUserWidget>(GetWorld(), BloodWidgetClass);
		if (BloodEffectWidget)
		{
			BloodEffectWidget->AddToViewport();
		}
	}

}

void ACultistCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &ACultistCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ACultistCharacter::MoveRight);
	PlayerInputComponent->BindAction("PerformRitual", IE_Pressed, this, &ACultistCharacter::StartRitual);


	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ACultistCharacter::ToggleCrouch);
	
	PlayerInputComponent->BindAction("SpecialAbility", IE_Pressed, this, &ACultistCharacter::StartPreviewPlacement);
	PlayerInputComponent->BindAction("ConfirmPlacement", IE_Pressed, this, &ACultistCharacter::ConfirmPlacement);
	UE_LOG(LogTemp, Warning, TEXT("Binding  input"));

}

void ACultistCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		FRotator ControlRotation = GetControlRotation();
		FRotator YawRotation(0, ControlRotation.Yaw, 0);

		FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		//UE_LOG(LogTemp, Warning, TEXT("MoveForward: %f"), Value);
		//UE_LOG(LogTemp, Warning, TEXT("WalkSpeed: %f"), GetCharacterMovement()->MaxWalkSpeed);
		//UE_LOG(LogTemp, Warning, TEXT("Movement Mode: %d"), GetCharacterMovement()->MovementMode);
		//UE_LOG(LogTemp, Warning, TEXT("Is Falling: %d"), GetCharacterMovement()->IsFalling());
		//UE_LOG(LogTemp, Warning, TEXT("Is Moving On Ground: %d"), GetCharacterMovement()->IsMovingOnGround());
		AddMovementInput(Direction, Value);
	}
}

void ACultistCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		FRotator ControlRotation = GetControlRotation();
		FRotator YawRotation(0, ControlRotation.Yaw, 0);

		FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
}

void ACultistCharacter::ToggleCrouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void ACultistCharacter::PerformRitual()
{
	if (!bIsPerformingRitual) return;

	TaskRitualProgress += TaskRitualSpeed * 0.1f; // 의식 작업 진행도
	
	if (TaskRitualWidget)
	{
		UProgressBar* ProgressBar = Cast<UProgressBar>(TaskRitualWidget->GetWidgetFromName(TEXT("RitualProgressBar")));
		if (ProgressBar)
		{
			ProgressBar->SetPercent(TaskRitualProgress / 100.0f);
			UE_LOG(LogTemp, Warning, TEXT("TaskProgressBar Exist"));
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("Performing Ritual..."));
	UE_LOG(LogTemp, Warning, TEXT("TaskRitualProgress: %f"), TaskRitualProgress);
	if (TaskRitualProgress >= 100.0f)
	{
		if (CurrentAltar)
		{
			CurrentAltar->IncreaseRitualGauge();
		}
		StopRitual();
	}


	// Check 100%
	//if (RitualProgress >= 100.0f)
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("Ritual Completed!"));
	//}


}

void ACultistCharacter::StartRitual()
{
	if (bIsPerformingRitual)
	{
		UE_LOG(LogTemp, Warning, TEXT("Already Performing.."));
		return;
	}
	if (!CurrentAltar) { 
		UE_LOG(LogTemp, Warning, TEXT("No altar Here!"));
		//UE_LOG(LogTemp, Warning, TEXT("%f"), GetCharacterMovement()->MaxWalkSpeed);
		return;
	}

	bIsPerformingRitual = true;
	TaskRitualProgress = 0.0f;
	if (TaskRitualWidget)
	{
		TaskRitualWidget->SetVisibility(ESlateVisibility::Visible);
	}
	
	GetWorld()->GetTimerManager().SetTimer(RitualTimerHandle, this, &ACultistCharacter::PerformRitual, 0.1f, true);
	UE_LOG(LogTemp, Warning, TEXT("Ritual Started"));

	GetCharacterMovement()->DisableMovement();

	// Animation
}

// 중단 x, 완료처리
void ACultistCharacter::StopRitual()
{
	if (!bIsPerformingRitual)return;

	bIsPerformingRitual = false;
	GetWorld()->GetTimerManager().ClearTimer(RitualTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(TaskRitualTimerHandle);

	UE_LOG(LogTemp, Warning, TEXT("Ritual Stopped"));
	RitualProgress += RitualSpeed;	// 전체 의식게이지

	if (TaskRitualWidget)
	{
		TaskRitualWidget->SetVisibility(ESlateVisibility::Hidden);
		TaskRitualProgress = 0.0f;
	}

	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ACultistCharacter::CancelRitual()
{
	UE_LOG(LogTemp, Warning, TEXT("Ritual Canceled"));

	bIsPerformingRitual = false;
	GetWorld()->GetTimerManager().ClearTimer(RitualTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(TaskRitualTimerHandle);

	if (TaskRitualWidget)
	{
		TaskRitualWidget->SetVisibility(ESlateVisibility::Hidden);
		TaskRitualProgress = 0.0f;
	}

	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ACultistCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if(SpawnedPreviewActor)
	{
		UpdatePreviewPlacement();

		// 사정거리 시각화 원
		DrawDebugCircle(
			GetWorld(),
			GetActorLocation(),
			MaxPlacementDistance,
			64,
			FColor::Green,
			false,
			0.01f,
			0,
			1.f,
			FVector(1, 0, 0),
			FVector(0, 1, 0),
			false
		);
	}
}

void ACultistCharacter::SetCurrentAltar(AAltar* Altar)
{
	CurrentAltar = Altar;
}

void ACultistCharacter::TakeDamage(float DamageAmount)
{
	if (bIsStunned)return;
	bIsHitByAnAttack = true;
	
	if (bIsPerformingRitual)
	{
		CancelRitual();
	}

	// 피격 이펙트
	if (BloodEffectWidget)
	{
		UFunction* PlayFunc = BloodEffectWidget->FindFunction(FName("PlayBloodEffect"));
		if (PlayFunc)
		{
			BloodEffectWidget->ProcessEvent(PlayFunc, nullptr);
		}
	}

	GetCharacterMovement()->MaxWalkSpeed = 100.0f;
	// 잠시 둔화
	//GetCharacterMovement()->MaxWalkSpeed *= 0.4f;


	// 피격 리액션 타이머
	GetWorld()->GetTimerManager().SetTimer(HitByAttackTH, this, &ACultistCharacter::GottaRun, 0.8f, false);
	
	// 데미지 처리
	CurrentHealth -= DamageAmount;
	UE_LOG(LogTemp, Warning, TEXT("HP : %f Now"), CurrentHealth);

	if (CurrentHealth <= 0)
	{
		if (bIsAlreadyStunned)
		{
			Die();
		}
		else
		{
			Stun();
		}
	}
}

void ACultistCharacter::OnHitbyBaton(const FVector& AttackerLocation, float AttackDamage) {
	FVector Direction = (GetActorLocation() - AttackerLocation).GetSafeNormal();
	float Distance = FVector::Dist(AttackerLocation, GetActorLocation());
	// 거리에 반비례
	float MaxDistance = 200.0f;
	float MinPush = 300.0f;	// 최대 거리 히트 시 넉백속력
	float MaxPush = 800.0f;

	float Alpha = 1.0f - FMath::Clamp(Distance / MaxDistance, 0.0f, 1.0f);
	float FinalPushStrength = FMath::Lerp(MinPush, MaxPush, Alpha);

	FVector LaunchVelocity = Direction * FinalPushStrength + FVector(0.0f, 0.0f, 100.0f);

	// 장애물 유무로 거리제한
	FVector Start = GetActorLocation();
	FVector End = Start + Direction * FinalPushStrength;
	// 장애물 유무판별 라인트레이스
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	bool SomethingBehind = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	if (SomethingBehind)
	{
		float WallDistance = FVector::Dist(Start, Hit.ImpactPoint);
		LaunchVelocity = Direction * WallDistance * 0.9f + FVector(0.f, 0.f, 100.f);
	}
	// 최종 넉백
	LaunchCharacter(LaunchVelocity, true, true);

	// 데미지 처리
	TakeDamage(AttackDamage);
}

void ACultistCharacter::GottaRun()
{
	UE_LOG(LogTemp, Warning, TEXT("Cultist Gotta Run"));
	bIsHitByAnAttack = false;

	//GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->MaxWalkSpeed = 600.0f;	// 속도 복구
	GetCharacterMovement()->GroundFriction = 8.0f; // 마찰율 복구
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

void ACultistCharacter::Die()
{
	UE_LOG(LogTemp, Warning, TEXT("Died."));

	bIsDead = true;
	GetCharacterMovement()->DisableMovement();

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		DisableInput(PC);
		GetWorldTimerManager().SetTimer(DisableTimerHandle, this, &ACultistCharacter::SendDisableToServer, 5.0f, false);
	}
	//if (ACultGameMode* GM = Cast<ACultGameMode>(UGameplayStatics::GetGameMode(this)))
	//{
	//	GM->CheckPoliceVictoryCondition();	// 죽었을때, 감금당했을 때 체크
	//}
}

void ACultistCharacter::Stun()
{
	if (bIsAlreadyStunned)
	{
		Die();
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("Got Stunned"));
	bIsStunned = true;
	UE_LOG(LogTemp, Warning, TEXT("Stun called %d Now"), bIsStunned);
	bIsElectric = false;

	GetCharacterMovement()->DisableMovement();

	//AI 방지
	GetCharacterMovement()->MaxWalkSpeed = 0.0f;

	// 캡슐충돌x
	//GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);



	//IntoRagdoll();

	// 일정 시간 후 깨어남
	GetWorld()->GetTimerManager().SetTimer(ReviveTimerHandle, this, &ACultistCharacter::GetUp, 10.0f, false);
}

void ACultistCharacter::Revive()
{
	if (!bIsStunned)return;

	bIsStunned = false;
	bIsAlreadyStunned = true;
	TurnToStun = false;
	//// 물리시뮬Off, 충돌 복구
	//GetMesh()->SetSimulatePhysics(false);
	//GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
	//GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	//
	//// 메시-캡슐 부착
	//GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	//
	//// 메시 위치, 회전 초기화	GetActorRotation().Yaw
	//GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	//GetMesh()->SetRelativeLocation(FVector(0, 0, -100));
	//// 위치복구 확인
	//UE_LOG(LogTemp, Warning, TEXT("Mesh Re;ativeLocation: %s"), *GetMesh()->GetRelativeLocation().ToString());

	//// 애니메이션 복구
	//GetMesh()->bPauseAnims = false;
	//GetMesh()->bNoSkeletonUpdate = false;
	
	// 일정 체력 남은상태로 복구
	Health = 50.0f;

	// 이동 가능한 상태로 복구
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->MaxWalkSpeed = 600.0f;
	UE_LOG(LogTemp, Warning, TEXT("Revive."));

}

void ACultistCharacter::IntoRagdoll()
{
	// Ragdoll 효과

	// - 물리 전환
	GetMesh()->SetSimulatePhysics(true);	// 물리시뮬On
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->bBlendPhysics = true;
	GetMesh()->WakeAllRigidBodies();
	
	// 애니메이션 pause
	FTimerHandle PauseHandle;
	GetWorld()->GetTimerManager().SetTimer(PauseHandle, [this]()
		{
			GetMesh()->bPauseAnims = true;
			GetMesh()->bNoSkeletonUpdate = true;
		}, 0.05f, false);
}

void ACultistCharacter::TakeMeleeDamage(float DamageAmount)
{
	TakeDamage(DamageAmount);
	// 경직
}

void ACultistCharacter::TakePistolDamage(float DamageAmount)
{
	TakeDamage(DamageAmount);
	// 피격

}

void ACultistCharacter::GotHitTaser(AActor* Attacker)
{
	// 기절중 - 무효, 피격중 - 유효
	if (bIsStunned)return;
	
	// 이동불가 (감전->기절과정 중)
	GetCharacterMovement()->DisableMovement();

	// 노티파이로 감전애니메이션에 Stun
	// 
	// 감전
	bIsElectric = true;
	
	// 피격 이펙트
	if (BloodEffectWidget)
	{
		UFunction* PlayFunc = BloodEffectWidget->FindFunction(FName("PlayBloodEffect"));
		if (PlayFunc)
		{
			BloodEffectWidget->ProcessEvent(PlayFunc, nullptr);
		}
	}

	// 방향 판단
	if (Attacker)
	{
		FVector AttackerDirection = (Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		float ForwardDot = FVector::DotProduct(GetActorForwardVector(), AttackerDirection);

		if (ForwardDot > 0)
		{
			bIsFrontFallen = false; // 앞에서 맞음 → 뒤로 넘어짐
		}
		else
		{
			bIsFrontFallen = true; // 뒤에서 맞음 → 앞으로 넘어짐
		}
	}

}

void ACultistCharacter::FallDown()
{
	//
	TurnToStun = true;
	Stun();
}

void ACultistCharacter::GetUp()
{
	TurnToGetUp = true;
	UE_LOG(LogTemp, Warning, TEXT("Now Get Up"));
	GetWorld()->GetTimerManager().SetTimer(ReviveTimerHandle, this, &ACultistCharacter::Revive, 3.0f, false);
}

bool ACultistCharacter::IsInactive() const
{
	return bIsDead || bIsConfined;
}

// 이동 시 중단 but 이동불가로 설정
/*
void ACultistCharacter::MoveForward(float Value)
{
	if (Value != 0.0f) StopRitual();
}*/



// Skill, Abilities		================================================
void ACultistCharacter::StartPreviewPlacement()
{
	UE_LOG(LogTemp, Warning, TEXT("StartPreviewPlacement"));
	if (GrowthPreviewActorClass && !SpawnedPreviewActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnPreviewActor"));
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnedPreviewActor = GetWorld()->SpawnActor<AGrowthPreviewActor>(GrowthPreviewActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}
}

void ACultistCharacter::UpdatePreviewPlacement()
{
	if(!SpawnedPreviewActor) {
		UE_LOG(LogTemp, Warning, TEXT("No SpawnPreviewActor"));
		return;
	}

	FHitResult Hit;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)return;

	// 커서위치 -> 월드에 트레이스
	PC->GetHitResultUnderCursor(PlacementCheckChannel, false, Hit);

	if (Hit.bBlockingHit)
	{
		FVector TargetLocation = Hit.Location;
		AActor* HitActor = Hit.GetActor();

		bool bWithinRange = FVector::Dist(GetActorLocation(), TargetLocation) <= MaxPlacementDistance;
		//bool bValidSurface = (HitActor && HitActor->IsA(ALandscape::StaticClass()) || HitActor->ActorHasTag(TEXT("Ground")));

		SpawnedPreviewActor->UpdatePreviewLocation(TargetLocation);

		bool bHasCollision = GetWorld()->OverlapBlockingTestByChannel(
			TargetLocation, FQuat::Identity,
			ECC_Visibility, FCollisionShape::MakeSphere(50.f)
		);

		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		bool bOverlap = GetWorld()->OverlapBlockingTestByChannel(
			TargetLocation, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeSphere(50.f), Params
		);

		bCanPlace = bWithinRange && !bOverlap; //&& !bHasCollision;	// && bValideSurface
		SpawnedPreviewActor->SetValidPlacement(bCanPlace);

		// 가시성 설정 
		SpawnedPreviewActor->SetActorHiddenInGame(false);
	}
	else
	{
		SpawnedPreviewActor->SetActorHiddenInGame(true);
	}
}

void ACultistCharacter::ConfirmPlacement()
{
	if (!SpawnedPreviewActor)return;
	if (!bCanPlace) return;

	FVector SpawnLocation = SpawnedPreviewActor->GetActorLocation();
	FRotator SpawnRotation = SpawnedPreviewActor->GetActorRotation();

	if (TreeObstacleActorClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;

		GetWorld()->SpawnActor<ATreeObstacleActor>(TreeObstacleActorClass, SpawnLocation, SpawnRotation, SpawnParams);
	}

	SpawnedPreviewActor->Destroy();
	SpawnedPreviewActor = nullptr;
}




void ACultistCharacter::TurnCamera(float Value)
{
	AddControllerYawInput(Value);
}
void ACultistCharacter::LookUpCamera(float Value)
{
	AddControllerPitchInput(Value);
}

int ACultistCharacter::GetPlayerID() const {
	return my_ID;
}

void ACultistCharacter::SendDisableToServer()
{
	if (MySocketCultistActor)
	{
		MySocketCultistActor->SendDisable();
	}
}