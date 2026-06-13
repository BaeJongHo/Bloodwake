// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BWTargetingComponent.generated.h"

class ABWPlayerCharacter;
class APlayerController;
class USpringArmComponent;

/**
 * 플레이어에 부착하는 락온(Lock-On) 타겟팅 핵심 컴포넌트.
 *
 * 책임:
 * - 락온 토글(ToggleLockOn): 카메라 전방 구체 스윕으로 후보를 수집해 최적 타깃을 선택한다.
 * - 타깃 전환(SwitchTargetWithDirection): 좌/우 방향으로 현재 타깃을 교체한다.
 * - 카메라 보간(TickComponent): 락온 중에만 Tick 활성화하여 ControlRotation을 타깃 방향으로 RInterpTo 보간.
 * - 스트레이프 모드: 락온 시 bUseControllerRotationYaw=true / bOrientRotationToMovement=false 로 전환,
 *   해제 시 원복.
 * - 자동 해제: 매 틱 ValidateCurrentTarget으로 사망/거리 초과/LOS 끊김 감지 시 EndLockOn.
 *
 * Tick은 기본 비활성(bStartWithTickEnabled=false). 락온 시작 시에만 SetComponentTickEnabled(true).
 * 싱글플레이 전용 — 리플리케이션/HasAuthority 코드 없음.
 */
UCLASS(Blueprintable, ClassGroup = (Bloodwake), meta = (BlueprintSpawnableComponent))
class BLOODWAKE_API UBWTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBWTargetingComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ── 공개 API ────────────────────────────────────────────────────────

	/** IA_LockOn 콜백에서 호출한다. 락온 중이면 EndLockOn, 아니면 StartLockOn. */
	UFUNCTION(BlueprintCallable, Category = "Combat|LockOn")
	void ToggleLockOn();

	/**
	 * 좌(AxisX < 0) / 우(AxisX > 0) 방향으로 타깃을 전환한다.
	 * IA_SwitchTarget 콜백에서 입력 X축 값을 전달한다.
	 * 데드존(SwitchInputDeadzone) 미만 입력은 무시한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|LockOn")
	void SwitchTargetWithDirection(float AxisX);

	/** 현재 락온 중인지 반환한다. AnimInstance / BP에서 조회한다. */
	UFUNCTION(BlueprintPure, Category = "Combat|LockOn")
	bool IsLockedOn() const { return CurrentTarget.IsValid(); }

	/** 현재 타깃 액터를 반환한다. 없으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "Combat|LockOn")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	// ── BP/디자이너 설정용 튜닝 데이터 ─────────────────────────────────

	/** 락온 최대 거리(cm). 이 거리를 초과하면 자동 해제된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|LockOn", meta = (ClampMin = "0.0"))
	float MaxLockOnDistance = 1500.f;

	/** 후보 수집 스윕 구체 반경(cm). 카메라 전방 원통형 탐색 폭. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|LockOn", meta = (ClampMin = "0.0"))
	float SweepRadius = 600.f;

	/** 화면 중앙 기준 최대 선택 각도(도). 이 시야각 밖의 적은 후보에서 제외된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|LockOn", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float MaxSelectionAngleDeg = 40.f;

	/** 카메라 → 타깃 회전 보간 속도(RInterpTo speed). 클수록 빠르게 타깃을 따라간다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|LockOn", meta = (ClampMin = "0.0"))
	float CameraInterpSpeed = 8.f;

	/** 락온 카메라 피치 오프셋(도). 양수일수록 구도를 위로 올린다(타깃을 화면 위쪽에 둠). 0이면 타깃 정조준. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|LockOn", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
	float LockOnPitchOffset = 8.f;

	/** 타깃 전환 시 입력 절대값 데드존. 이 값 미만의 입력은 무시된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|LockOn", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SwitchInputDeadzone = 0.5f;

	/** LOS(벽 가림) 체크 사용 여부. true이면 카메라→타깃 Visibility 트레이스로 가림을 확인한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|LockOn")
	bool bUseLineOfSight = true;

	/** 후보 수집 스윕 채널. 기본 Targeting(ECC_GameTraceChannel1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|LockOn")
	TEnumAsByte<ECollisionChannel> TargetingChannel = ECC_GameTraceChannel1;

	/** LOS 체크용 채널. 기본 Visibility. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|LockOn")
	TEnumAsByte<ECollisionChannel> LineOfSightChannel = ECC_Visibility;

	/** 디버그 드로우 활성화 여부. 개발 중 스윕/트레이스 확인에 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|LockOn")
	bool bDrawDebug = false;

protected:
	/** 후보를 수집해 최적 타깃을 선택하고 락온 상태에 진입한다. */
	void StartLockOn();

	/** 타깃 해제 + 마커 off + 스트레이프 모드 복귀 + Tick 비활성화. */
	void EndLockOn();

	/**
	 * 카메라 전방 SphereSweepMultiByChannel로 후보를 수집한다.
	 * IBWTargetingInterface 구현 + CanBeTargeted() 통과 액터만 Out에 추가한다.
	 */
	void CollectCandidates(TArray<AActor*>& Out) const;

	/**
	 * 후보 중 화면 중앙각 기준으로 최적 타깃을 반환한다.
	 * 없으면 nullptr.
	 */
	AActor* FindBestTarget() const;

	/**
	 * 거리/시야각/LOS/CanBeTargeted 필터를 통과하는지 확인한다.
	 * 통과 시 OutScreenAngleDeg에 카메라 전방과의 각도(도)를 반환한다.
	 * ExcludeActor가 지정되면 그 액터는 후보에서 제외한다.
	 */
	bool PassesTargetFilters(AActor* Candidate, float& OutScreenAngleDeg,
		const AActor* ExcludeActor = nullptr) const;

	/** 타깃이 여전히 유효한지(CanBeTargeted + 거리 + LOS) 확인한다. */
	bool IsValidTarget(AActor* Actor) const;

	/** 매 Tick: 타깃 유효성 검사 → 유효하면 카메라 보간. */
	void ValidateCurrentTarget();

	/** ControlRotation을 타깃 방향으로 RInterpTo 보간한다. */
	void UpdateCameraToTarget(float DeltaTime);

	/** 스트레이프 모드 진입: bUseControllerRotationYaw=true / bOrientRotationToMovement=false. */
	void EnterStrafeMode();

	/** 스트레이프 모드 해제: 진입 전 값으로 원복. */
	void ExitStrafeMode();

	// ── 런타임 캐시 (GC 추적 필수) ──────────────────────────────────────

	/** 현재 타깃. 소유하지 않는 참조이므로 TWeakObjectPtr. 적 Destroy 시 자동 무효화. */
	UPROPERTY(VisibleInstanceOnly, Category = "Combat|LockOn")
	TWeakObjectPtr<AActor> CurrentTarget;

	/** 소유 플레이어 캐릭터 캐시. BeginPlay에서 1회 설정. */
	UPROPERTY(Transient)
	TObjectPtr<ABWPlayerCharacter> OwnerCharacter;

	/** 플레이어 컨트롤러 캐시. SetControlRotation에 사용. */
	UPROPERTY(Transient)
	TObjectPtr<APlayerController> CachedController;

	/** SpringArm 캐시. 회전/콜리전 복원용. */
	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CachedSpringArm;

	/** 팔로우 카메라 캐시. 위치/전방 벡터 조회에 사용. BeginPlay에서 1회 캐시(4.1 규약). */
	UPROPERTY(Transient)
	TObjectPtr<class UCameraComponent> CachedCamera;

	// ── 비-UObject 런타임 상태 (UPROPERTY 불필요) ────────────────────────

	/** EnterStrafeMode 진입 전 bOrientRotationToMovement 저장값. */
	bool bPrevOrientRotationToMovement = true;

	/** EnterStrafeMode 진입 전 bUseControllerRotationYaw 저장값. */
	bool bPrevUseControllerRotationYaw = false;
};
