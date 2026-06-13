// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWTargetingComponent.h"

#include "Character/BWPlayerCharacter.h"
#include "Combat/BWTargetingInterface.h"
#include "Combat/BWTargetingCollisionComponent.h"
#include "Combat/BWLockOnWidgetComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogBWTargeting, Log, All);

UBWTargetingComponent::UBWTargetingComponent()
{
	// Tick은 기본 켜두되 시작 시 비활성 — 락온 시작 시에만 SetComponentTickEnabled(true).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UBWTargetingComponent::BeginPlay()
{
	Super::BeginPlay();

	// 소유 캐릭터 캐시 (생성자에서 하면 월드 없어 null — BeginPlay에서 수행).
	OwnerCharacter = Cast<ABWPlayerCharacter>(GetOwner());
	if (!ensure(IsValid(OwnerCharacter)))
	{
		UE_LOG(LogBWTargeting, Warning,
			TEXT("[BWTargetingComponent] 소유자가 ABWPlayerCharacter가 아닙니다. (%s)"),
			*GetOwner()->GetName());
		return;
	}

	// 컨트롤러 캐시
	CachedController = Cast<APlayerController>(OwnerCharacter->GetController());

	// SpringArm / Camera 캐시 — FindComponentByClass는 BeginPlay에서 1회만 수행한다(4.1 규약).
	CachedSpringArm = OwnerCharacter->FindComponentByClass<USpringArmComponent>();
	CachedCamera    = OwnerCharacter->FindComponentByClass<UCameraComponent>();

	if (!CachedCamera)
	{
		UE_LOG(LogBWTargeting, Warning,
			TEXT("[BWTargetingComponent] FollowCamera를 찾을 수 없습니다. 카메라 보간이 제한됩니다."));
	}

	// Tick은 명시적으로 비활성 확인 (생성자 설정 보강)
	SetComponentTickEnabled(false);
}

void UBWTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 락온 중이면 회전 모드를 원복하고 Tick을 끈다(수명 종료 안전망).
	if (IsLockedOn())
	{
		EndLockOn();
	}

	Super::EndPlay(EndPlayReason);
}

void UBWTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ValidateCurrentTarget이 EndLockOn을 호출하면 Tick이 꺼지므로 이후 코드는 실행되지 않는다.
	ValidateCurrentTarget();

	if (IsLockedOn())
	{
		UpdateCameraToTarget(DeltaTime);
	}
}

// ── 공개 API ─────────────────────────────────────────────────────────────────

void UBWTargetingComponent::ToggleLockOn()
{
	if (IsLockedOn())
	{
		EndLockOn();
	}
	else
	{
		StartLockOn();
	}
}

void UBWTargetingComponent::SwitchTargetWithDirection(float AxisX)
{
	// 데드존 미만 입력 무시
	if (FMath::Abs(AxisX) < SwitchInputDeadzone)
	{
		return;
	}

	if (!IsLockedOn())
	{
		return;
	}

	AActor* OldTarget = CurrentTarget.Get();

	// 현재 타깃을 제외한 후보 재수집
	TArray<AActor*> Candidates;
	CollectCandidates(Candidates);

	// 현재 타깃 제거
	Candidates.RemoveSingleSwap(OldTarget);

	if (Candidates.IsEmpty())
	{
		return;
	}

	// 카메라 우벡터 계산 — AxisX > 0이면 오른쪽, < 0이면 왼쪽
	FVector CameraRightVec = FVector::ZeroVector;
	if (IsValid(CachedController))
	{
		const FRotator ControlRot = CachedController->GetControlRotation();
		const FRotationMatrix RotMat(FRotator(0.f, ControlRot.Yaw, 0.f));
		CameraRightVec = RotMat.GetScaledAxis(EAxis::Y);
	}

	// 카메라 위치 획득 (CachedCamera는 BeginPlay에서 1회 캐시됨)
	FVector CameraLocation = IsValid(OwnerCharacter) ? OwnerCharacter->GetActorLocation() : FVector::ZeroVector;
	if (IsValid(CachedCamera))
	{
		CameraLocation = CachedCamera->GetComponentLocation();
	}

	// 현재 타깃 방향의 우벡터 성분 부호와 일치하는 후보만 분류
	AActor* BestCandidate = nullptr;
	float BestAngle = TNumericLimits<float>::Max();

	for (AActor* Candidate : Candidates)
	{
		if (!IsValid(Candidate))
		{
			continue;
		}

		// 후보가 왼쪽/오른쪽인지 판별
		const FVector ToCandidate = (Candidate->GetActorLocation() - CameraLocation).GetSafeNormal();
		const float RightDot = FVector::DotProduct(CameraRightVec, ToCandidate);

		// AxisX > 0이면 오른쪽(RightDot > 0), AxisX < 0이면 왼쪽(RightDot < 0)
		const bool bOnCorrectSide = (AxisX > 0.f) ? (RightDot > 0.f) : (RightDot < 0.f);
		if (!bOnCorrectSide)
		{
			continue;
		}

		float ScreenAngle = 0.f;
		if (!PassesTargetFilters(Candidate, ScreenAngle, OldTarget))
		{
			continue;
		}

		if (ScreenAngle < BestAngle)
		{
			BestAngle = ScreenAngle;
			BestCandidate = Candidate;
		}
	}

	if (!BestCandidate)
	{
		return;
	}

	// 이전 타깃 마커 끄기
	if (IsValid(OldTarget))
	{
		if (IBWTargetingInterface* OldInterface = Cast<IBWTargetingInterface>(OldTarget))
		{
			if (UBWLockOnWidgetComponent* OldWidget = OldInterface->GetLockOnWidgetComponent())
			{
				OldWidget->HideMarker();
			}
		}
	}

	// 새 타깃 설정 및 마커 켜기
	CurrentTarget = BestCandidate;
	if (IBWTargetingInterface* NewInterface = Cast<IBWTargetingInterface>(BestCandidate))
	{
		if (UBWLockOnWidgetComponent* NewWidget = NewInterface->GetLockOnWidgetComponent())
		{
			NewWidget->ShowMarker();
		}
	}

	UE_LOG(LogBWTargeting, Log, TEXT("[BWTargetingComponent] 타깃 전환: %s"), *BestCandidate->GetName());
}

// ── 내부 구현 ────────────────────────────────────────────────────────────────

void UBWTargetingComponent::StartLockOn()
{
	AActor* BestTarget = FindBestTarget();
	if (!BestTarget)
	{
		UE_LOG(LogBWTargeting, Log, TEXT("[BWTargetingComponent] 락온 후보 없음."));
		return;
	}

	CurrentTarget = BestTarget;

	// 마커 표시
	if (IBWTargetingInterface* TargetInterface = Cast<IBWTargetingInterface>(BestTarget))
	{
		if (UBWLockOnWidgetComponent* Widget = TargetInterface->GetLockOnWidgetComponent())
		{
			Widget->ShowMarker();
		}
	}

	EnterStrafeMode();

	// Tick 활성화 — 카메라 보간 시작
	SetComponentTickEnabled(true);

	// 캐릭터의 캐시 bool 동기화 (AnimInstance 워커 스레드 안전을 위해)
	if (IsValid(OwnerCharacter))
	{
		OwnerCharacter->SetIsLockedOnCached(true);
	}

	UE_LOG(LogBWTargeting, Log, TEXT("[BWTargetingComponent] 락온 시작: %s"), *BestTarget->GetName());
}

void UBWTargetingComponent::EndLockOn()
{
	// 마커 숨기기
	if (AActor* Target = CurrentTarget.Get())
	{
		if (IBWTargetingInterface* TargetInterface = Cast<IBWTargetingInterface>(Target))
		{
			if (UBWLockOnWidgetComponent* Widget = TargetInterface->GetLockOnWidgetComponent())
			{
				Widget->HideMarker();
			}
		}
	}

	CurrentTarget.Reset();
	ExitStrafeMode();

	// Tick 비활성화
	SetComponentTickEnabled(false);

	// 캐릭터의 캐시 bool 동기화 (AnimInstance 워커 스레드 안전을 위해)
	if (IsValid(OwnerCharacter))
	{
		OwnerCharacter->SetIsLockedOnCached(false);
	}

	UE_LOG(LogBWTargeting, Log, TEXT("[BWTargetingComponent] 락온 해제."));
}

void UBWTargetingComponent::CollectCandidates(TArray<AActor*>& Out) const
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(OwnerCharacter))
	{
		return;
	}

	// 카메라 위치/전방 획득 (CachedCamera는 BeginPlay에서 1회 캐시됨)
	FVector CameraLocation = OwnerCharacter->GetActorLocation();
	FVector CameraForward = OwnerCharacter->GetActorForwardVector();

	if (IsValid(CachedCamera))
	{
		CameraLocation = CachedCamera->GetComponentLocation();
		CameraForward  = CachedCamera->GetForwardVector();
	}

	const FVector SweepEnd = CameraLocation + CameraForward * MaxLockOnDistance;
	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(SweepRadius);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BWTargetingCollect), false, OwnerCharacter.Get());

	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(Hits, CameraLocation, SweepEnd,
		FQuat::Identity, TargetingChannel, SweepShape, Params);

	if (bDrawDebug)
	{
		DrawDebugSphere(World, SweepEnd, SweepRadius, 12, FColor::Cyan, false, 1.0f);
		DrawDebugLine(World, CameraLocation, SweepEnd, FColor::Cyan, false, 1.0f, 0, 2.f);
	}

	// IBWTargetingInterface 구현 + CanBeTargeted() 통과 액터만 수집 (중복 제거)
	TSet<AActor*> UniqueActors;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor) || UniqueActors.Contains(HitActor))
		{
			continue;
		}

		if (IBWTargetingInterface* Interface = Cast<IBWTargetingInterface>(HitActor))
		{
			if (Interface->CanBeTargeted())
			{
				Out.Add(HitActor);
				UniqueActors.Add(HitActor);
			}
		}
	}
}

AActor* UBWTargetingComponent::FindBestTarget() const
{
	TArray<AActor*> Candidates;
	CollectCandidates(Candidates);

	AActor* BestTarget = nullptr;
	float BestAngle = TNumericLimits<float>::Max();

	for (AActor* Candidate : Candidates)
	{
		float ScreenAngle = 0.f;
		if (PassesTargetFilters(Candidate, ScreenAngle))
		{
			if (ScreenAngle < BestAngle)
			{
				BestAngle = ScreenAngle;
				BestTarget = Candidate;
			}
		}
	}

	return BestTarget;
}

bool UBWTargetingComponent::PassesTargetFilters(AActor* Candidate, float& OutScreenAngleDeg,
	const AActor* ExcludeActor) const
{
	if (!IsValid(Candidate) || Candidate == ExcludeActor)
	{
		return false;
	}

	IBWTargetingInterface* Interface = Cast<IBWTargetingInterface>(Candidate);
	if (!Interface || !Interface->CanBeTargeted())
	{
		return false;
	}

	if (!IsValid(OwnerCharacter))
	{
		return false;
	}

	// 카메라 위치/전방 획득 (CachedCamera는 BeginPlay에서 1회 캐시됨)
	FVector CameraLocation = OwnerCharacter->GetActorLocation();
	FVector CameraForward  = OwnerCharacter->GetActorForwardVector();

	if (IsValid(CachedCamera))
	{
		CameraLocation = CachedCamera->GetComponentLocation();
		CameraForward  = CachedCamera->GetForwardVector();
	}

	// (a) 거리 체크
	const FVector FocusLocation = Interface->GetTargetFocusLocation();
	const float Distance = FVector::Dist(CameraLocation, FocusLocation);
	if (Distance > MaxLockOnDistance)
	{
		return false;
	}

	// (b) 시야각 체크 — 카메라 전방과 (카메라→타깃) 방향 사이 각도
	const FVector ToTarget = (FocusLocation - CameraLocation).GetSafeNormal();
	const float DotVal = FVector::DotProduct(CameraForward, ToTarget);
	// DotVal을 [-1, 1]로 클램프해 acos 도메인 오류 방지
	const float ClampedDot = FMath::Clamp(DotVal, -1.f, 1.f);
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(ClampedDot));

	if (AngleDeg > MaxSelectionAngleDeg)
	{
		return false;
	}

	// (c) LOS 체크 — 카메라→타깃 라인 트레이스
	if (bUseLineOfSight)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FHitResult LOSHit;
			FCollisionQueryParams LOSParams(SCENE_QUERY_STAT(BWTargetingLOS), false, OwnerCharacter.Get());
			LOSParams.AddIgnoredActor(Candidate);

			const bool bBlocked = World->LineTraceSingleByChannel(
				LOSHit, CameraLocation, FocusLocation, LineOfSightChannel, LOSParams);

			if (bBlocked)
			{
				if (bDrawDebug)
				{
					DrawDebugLine(GetWorld(), CameraLocation, FocusLocation, FColor::Red, false, 1.f, 0, 2.f);
				}
				return false;
			}

			if (bDrawDebug)
			{
				DrawDebugLine(GetWorld(), CameraLocation, FocusLocation, FColor::Green, false, 1.f, 0, 2.f);
			}
		}
	}

	OutScreenAngleDeg = AngleDeg;
	return true;
}

bool UBWTargetingComponent::IsValidTarget(AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	IBWTargetingInterface* Interface = Cast<IBWTargetingInterface>(Actor);
	if (!Interface || !Interface->CanBeTargeted())
	{
		return false;
	}

	float Dummy = 0.f;
	return PassesTargetFilters(Actor, Dummy);
}

void UBWTargetingComponent::ValidateCurrentTarget()
{
	if (!CurrentTarget.IsValid())
	{
		EndLockOn();
		return;
	}

	if (!IsValidTarget(CurrentTarget.Get()))
	{
		if (AActor* TargetRaw = CurrentTarget.Get())
		{
			UE_LOG(LogBWTargeting, Log,
				TEXT("[BWTargetingComponent] 타깃 유효성 실패 — 자동 해제: %s"),
				*TargetRaw->GetName());
		}
		EndLockOn();
	}
}

void UBWTargetingComponent::UpdateCameraToTarget(float DeltaTime)
{
	if (!IsValid(CachedController) || !CurrentTarget.IsValid())
	{
		return;
	}

	// 시선 기준점(피벗): ControlRotation은 스프링암 원점을 중심으로 회전하므로,
	// 방향 계산도 카메라 실제 위치가 아닌 피벗에서 해야 구도가 아래로 깔리지 않는다.
	FVector PivotLocation = OwnerCharacter->GetActorLocation();
	if (IsValid(CachedSpringArm))
	{
		PivotLocation = CachedSpringArm->GetComponentLocation();
	}

	// 타깃 포커스 위치
	FVector FocusLocation = CurrentTarget.Get()->GetActorLocation();
	if (IBWTargetingInterface* Interface = Cast<IBWTargetingInterface>(CurrentTarget.Get()))
	{
		FocusLocation = Interface->GetTargetFocusLocation();
	}

	// 피벗 → 타깃 방향으로 목표 회전 계산
	const FVector ToTarget = FocusLocation - PivotLocation;
	FRotator TargetRotation = ToTarget.Rotation();

	// 구도를 위로 올리는 피치 오프셋(양수 = 위). 소울라이크 표준 카메라 높이.
	TargetRotation.Pitch += LockOnPitchOffset;

	// Pitch 클램프: 너무 위/아래를 보지 않도록 제한 (소울라이크 표준 — 발/몸통 기준)
	TargetRotation.Pitch = FMath::Clamp(TargetRotation.Pitch, -60.f, 30.f);

	// 현재 ControlRotation에서 목표 방향으로 부드럽게 보간
	const FRotator CurrentRotation = CachedController->GetControlRotation();
	const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, CameraInterpSpeed);

	CachedController->SetControlRotation(NewRotation);
}

void UBWTargetingComponent::EnterStrafeMode()
{
	if (!IsValid(OwnerCharacter))
	{
		return;
	}

	// 진입 전 값 저장 (ExitStrafeMode에서 원복)
	bPrevUseControllerRotationYaw = OwnerCharacter->bUseControllerRotationYaw;

	if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
	{
		bPrevOrientRotationToMovement = MoveComp->bOrientRotationToMovement;

		// 스트레이프 모드: 캐릭터가 항상 컨트롤러(=타깃) 방향을 향함
		MoveComp->bOrientRotationToMovement = false;
	}

	OwnerCharacter->bUseControllerRotationYaw = true;

	UE_LOG(LogBWTargeting, Verbose, TEXT("[BWTargetingComponent] 스트레이프 모드 진입."));
}

void UBWTargetingComponent::ExitStrafeMode()
{
	if (!IsValid(OwnerCharacter))
	{
		return;
	}

	// 진입 전 값으로 원복
	OwnerCharacter->bUseControllerRotationYaw = bPrevUseControllerRotationYaw;

	if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = bPrevOrientRotationToMovement;
	}

	UE_LOG(LogBWTargeting, Verbose, TEXT("[BWTargetingComponent] 스트레이프 모드 해제."));
}
