// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Combat/BWCombatTypes.h"
#include "Combat/BWTargetingInterface.h"
#include "BWEnemy.generated.h"

class UBWAttributeComponent;
class UBWStateComponent;
class UBWTargetingCollisionComponent;
class UBWLockOnWidgetComponent;
class UAnimMontage;
class UParticleSystem;
class USoundBase;

/**
 * 피격을 받는 적 캐릭터 베이스 클래스.
 * TakeDamage 오버라이드로 데미지를 UBWAttributeComponent에 위임하고,
 * 피격 방향에 따른 4방향 히트 리액션 몽타주·VFX·사운드를 재생한다.
 * 사망 시(OnDeath 델리게이트) 랙돌로 전환하고 일정 시간 후 Destroy.
 * BP 자식(BP_Enemy...)에서 메시·몽타주·VFX·사운드를 지정한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS(Blueprintable)
class BLOODWAKE_API ABWEnemy : public ACharacter, public IBWTargetingInterface
{
	GENERATED_BODY()

public:
	ABWEnemy();

	/**
	 * 데미지 수신 오버라이드.
	 * FPointDamageEvent 에서 ImpactPoint/ShotDirection을 추출해
	 * AttributeComponent에 위임 + 히트 리액션·VFX·사운드를 재생한다.
	 */
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	// ── IBWTargetingInterface 구현 ───────────────────────────────────────

	/**
	 * 현재 타깃 가능 여부. bIsDead 또는 Character_State_Death 태그 보유 시 false.
	 * UBWTargetingComponent가 후보 필터/유효성 검사에서 매 틱 호출한다.
	 */
	virtual bool CanBeTargeted() const override;

	/** 락온 스윕이 맞히는 구체 콜리전 컴포넌트를 반환한다. */
	virtual UBWTargetingCollisionComponent* GetTargetingCollisionComponent() const override;

	/** 락온 마커 위젯 컴포넌트를 반환한다. */
	virtual UBWLockOnWidgetComponent* GetLockOnWidgetComponent() const override;

	/**
	 * 카메라가 바라볼 월드 위치(가슴 높이 기준).
	 * TargetingCollisionComponent 위치를 반환하고, 없으면 액터 위치 + 높이 오프셋 반환.
	 */
	virtual FVector GetTargetFocusLocation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** OnDeath 델리게이트 구독 콜백. UFUNCTION 필수(AddDynamic 요구). */
	UFUNCTION()
	void HandleDeath();

	// ── 컴포넌트 (플레이어 패턴과 동일) ────────────────────────────────

	/** Health / Stamina / Focus 자원 관리 컴포넌트. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes")
	TObjectPtr<UBWAttributeComponent> AttributeComponent;

	/** 현재 행동 상태를 GameplayTag로 관리하는 컴포넌트. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	TObjectPtr<UBWStateComponent> StateComponent;

	/**
	 * 락온 스윕이 맞히는 구체 형상 컴포넌트. Targeting 채널(ECC_GameTraceChannel1)에 Block.
	 * 반경/위치(가슴 높이)는 BP 자식(BP_BWEnemy)의 "(상속됨)" 컴포넌트에서 튜닝한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|LockOn")
	TObjectPtr<UBWTargetingCollisionComponent> TargetingCollision;

	/**
	 * Screen-space 락온 마커 위젯 컴포넌트. 타깃 선택 시 ShowMarker(), 해제 시 HideMarker().
	 * Widget Class는 BP 자식의 "(상속됨)" 컴포넌트에서 WBP_LockOn으로 지정한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|LockOn")
	TObjectPtr<UBWLockOnWidgetComponent> LockOnWidget;

	// ── BP 설정용 — 4방향 히트 리액션 몽타주 ──────────────────────────

	/** 정면 피격 히트 리액션 몽타주. BP 자식에서 AM_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|HitReaction")
	TObjectPtr<UAnimMontage> HitReactFrontMontage;

	/** 후방 피격 히트 리액션 몽타주. BP 자식에서 AM_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|HitReaction")
	TObjectPtr<UAnimMontage> HitReactBackMontage;

	/** 왼쪽 피격 히트 리액션 몽타주. BP 자식에서 AM_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|HitReaction")
	TObjectPtr<UAnimMontage> HitReactLeftMontage;

	/** 오른쪽 피격 히트 리액션 몽타주. BP 자식에서 AM_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|HitReaction")
	TObjectPtr<UAnimMontage> HitReactRightMontage;

	// ── BP 설정용 — 피격 VFX·사운드 ───────────────────────────────────

	/** 피격 Cascade 파티클 VFX. ImpactPoint에 스폰된다. BP 자식에서 P_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|HitEffects")
	TObjectPtr<UParticleSystem> HitVFX;

	/** 피격 사운드(MetaSound/SoundWave 등 USoundBase 호환). ImpactPoint에 재생. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|HitEffects")
	TObjectPtr<USoundBase> HitSound;

	// ── BP 설정용 — 사망·랙돌 ─────────────────────────────────────────

	/** 랙돌 전환 시 SkeletalMesh에 적용할 콜리전 프로파일명. 기본값 "Ragdoll". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Death")
	FName RagdollCollisionProfile = TEXT("Ragdoll");

	/** 사망 후 시체 잔존 시간(초). 0이면 Destroy 하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Death", meta = (ClampMin = "0.0"))
	float CorpseLifeSpan = 10.f;

	// ── 런타임 상태 ─────────────────────────────────────────────────────

	/** 사망 여부. TakeDamage 재진입 차단 및 HandleDeath 중복 진입 방지. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Death")
	bool bIsDead = false;

private:
	/**
	 * ShotDirection과 피격자 forward/right 내적으로 4방향을 분류한다.
	 * Front(±45°) / Back / Left / Right.
	 */
	EBWHitDirection ComputeHitDirection(const FVector& ShotDirection) const;

	/**
	 * 분류된 방향에 따라 해당 히트 리액션 몽타주를 재생한다.
	 * 해당 방향 몽타주가 null이면 Front로 폴백, Front도 null이면 무동작 + Warning.
	 */
	void PlayHitReaction(const FVector& ShotDirection);

	/**
	 * ImpactPoint에 Cascade 파티클 VFX 스폰 + 사운드 재생.
	 * HitVFX/HitSound가 설정되지 않으면 조용히 건너뛴다.
	 */
	void PlayHitEffects(const FVector& ImpactPoint);

	/**
	 * 랙돌 전환 — 사망 처리 순서:
	 * 1) bIsDead 가드, 2) StateComponent 초기화, 3) 몽타주 정지,
	 * 4) CharacterMovement 비활성화, 5) 캡슐 콜리전 OFF,
	 * 6) 메시 물리 ON, 7) CorpseLifeSpan 설정.
	 */
	void EnableRagdoll();
};
