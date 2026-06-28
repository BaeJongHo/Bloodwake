// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Combat/BWCombatTypes.h"
#include "Combat/BWTargetingInterface.h"
#include "Combat/BWCombatInterface.h"
#include "BWEnemy.generated.h"

class UBWAttributeComponent;
class UBWStateComponent;
class UBWCombatComponent;
class UBWAttackComponent;
class UBWTargetingCollisionComponent;
class UBWLockOnWidgetComponent;
class UBWEnemyHealthBarComponent;
class UAnimMontage;
class UParticleSystem;
class USoundBase;
class ATargetPoint;
class ABWWeapon;
class UDataTable;

/**
 * 피격을 받는 적 캐릭터 베이스 클래스.
 * TakeDamage 오버라이드로 데미지를 UBWAttributeComponent에 위임하고,
 * 피격 방향에 따른 4방향 히트 리액션 몽타주·VFX·사운드를 재생한다.
 * 사망 시(OnDeath 델리게이트) 랙돌로 전환하고 일정 시간 후 Destroy.
 * IBWCombatInterface 구현으로 BT(UBWBTTask_PerformAttack)가 공격을 요청할 수 있다.
 * BP 자식(BP_Enemy...)에서 메시·몽타주·VFX·사운드를 지정한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS(Blueprintable)
class BLOODWAKE_API ABWEnemy : public ACharacter, public IBWTargetingInterface, public IBWCombatInterface
{
	GENERATED_BODY()

public:
	ABWEnemy();

	/**
	 * 순찰 지점 배열 접근자. Patrol BT Task가 읽는다.
	 * const 참조 반환으로 복사 비용 없이 배열을 노출한다.
	 */
	const TArray<TObjectPtr<ATargetPoint>>& GetPatrolPoints() const { return PatrolPoints; }

	/**
	 * 플레이어 패링 성공 시 호출된다. 이 적을 Parried 상태로 전환하고 패링 당함 리액션 몽타주를 재생한다.
	 * 재생 시간 동안 이동이 차단되며, 타이머로 상태를 복구한다.
	 * bIsDead면 즉시 return.
	 */
	void Parried();

	/**
	 * 현재 스턴(Character.State.Stunned 태그 보유) 상태인지 반환한다.
	 * PerformAttack 가드 및 BT 서비스에서 사용한다.
	 */
	bool IsStunned() const;

	/**
	 * 현재 패링 당함(Character.State.Parried 태그 보유) 상태인지 반환한다.
	 * BT 서비스에서 사용한다.
	 */
	bool IsParried() const;

	/**
	 * 현재 손에 장착된 무기 액터를 반환한다. 패링 성공 VFX 스폰 위치로 사용한다.
	 * CombatComponent->GetEquippedWeapon()의 얇은 래퍼. 없으면 nullptr.
	 */
	AActor* GetEquippedWeaponActor() const;

	/**
	 * 데미지 수신 오버라이드.
	 * FPointDamageEvent 에서 ImpactPoint/ShotDirection을 추출해
	 * AttributeComponent에 위임 + 히트 리액션·VFX·사운드를 재생한다.
	 */
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	// ── IBWTargetingInterface 구현 ───────────────────────────────────────────

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

	// ── 공격 정리 (UBWBTTask_PerformAttack에서 사용) ──────────────────────────

	/**
	 * 진행 중인 공격 몽타주를 정지하고 공격 상태 태그를 초기화한다.
	 * UBWBTTask_PerformAttack::AbortTask에서 호출한다.
	 */
	void AbortCurrentAttack();

	/**
	 * 현재 공격 몽타주가 재생 중인지 여부.
	 * PerformAttack에서 ActiveAttackMontage를 설정하고 종료/중단 시 AbortCurrentAttack이 비우므로,
	 * 이 값이 곧 "공격 진행 중" 신호다.
	 * UBWBTService_SelectBehavior가 공격 중 행동 전환(모션 캔슬)을 막는 데 사용한다.
	 */
	bool IsAttacking() const { return ActiveAttackMontage != nullptr; }

	// ── HP 바 표시/숨김 API (ABWEnemyAIController가 호출) ──────────────────────

	/**
	 * 머리 위 HP 바를 표시한다.
	 * ABWEnemyAIController::UpdateTarget에서 플레이어를 감지하면 호출한다.
	 */
	void ShowHealthBar();

	/**
	 * 머리 위 HP 바를 숨긴다.
	 * ABWEnemyAIController::UpdateTarget에서 감지 종료 시, 또는 사망 시 호출한다.
	 */
	void HideHealthBar();

	// ── IBWCombatInterface 구현 ─────────────────────────────────────────────

	/**
	 * 공격을 1회 수행한다. BT(UBWBTTask_PerformAttack)가 호출한다.
	 * EnemyAttackDataTable에서 랜덤 Step을 선택해 몽타주를 재생하고,
	 * 종료 시 OnEnded를 호출해 BT Latent를 완료시킨다.
	 * 스태미나 부족·재생 실패 시에도 OnEnded를 즉시 호출(BT 교착 방지).
	 */
	virtual void PerformAttack(FOnMontageEnded OnEnded) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ── AI 순찰 데이터 ──────────────────────────────────────────────────────────

	/**
	 * 이 적이 순환 순찰할 지점들. 레벨에 배치한 ATargetPoint 액터를
	 * 디테일 패널에서 인스턴스별로 지정한다.
	 * 비어 있으면 Patrol Task는 즉시 Failed 반환(제자리 대기).
	 * EditInstanceOnly: 순찰 경로는 레벨 배치 인스턴스마다 다르다.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Enemy|AI")
	TArray<TObjectPtr<ATargetPoint>> PatrolPoints;

	/** OnDeath 델리게이트 구독 콜백. UFUNCTION 필수(AddDynamic 요구). */
	UFUNCTION()
	void HandleDeath();

	// ── 컴포넌트 ────────────────────────────────────────────────────────────

	/** Health / Stamina / Focus 자원 관리 컴포넌트. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes")
	TObjectPtr<UBWAttributeComponent> AttributeComponent;

	/** 현재 행동 상태를 GameplayTag로 관리하는 컴포넌트. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	TObjectPtr<UBWStateComponent> StateComponent;

	/** 장비 보유·장착·해제 로직을 담당하는 전투 컴포넌트. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBWCombatComponent> CombatComponent;

	/** 공격(콤보) 로직을 담당하는 컴포넌트. DataTable에서 몽타주·스태미나를 설정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBWAttackComponent> AttackComponent;

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

	/**
	 * Screen-space 머리 위 HP 바 위젯 컴포넌트.
	 * 평소 숨김, 플레이어 감지 시 ShowBar(), 감지 종료/사망 시 HideBar().
	 * Widget Class(WBP_EnemyHealthBar)는 BP 자식(BP_BWEnemy)의 "(상속됨)" 컴포넌트에서 지정한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UBWEnemyHealthBarComponent> HealthBarWidget;

	// ── BP 설정용 — 스턴(Stun) 데이터 ──────────────────────────────────

	/**
	 * 일반 피격 시 스턴에 빠질 확률(0~100, %). 0이면 절대 스턴 안 됨.
	 * BP 자식(BP_<적>)에서 적 종류마다 밸런싱한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Stun", meta = (ClampMin = "0", ClampMax = "100"))
	int32 StunnedRate = 0;

	/** 스턴 지속 추가 지연 최소값(초). HitReaction 몽타주 길이에 이 값을 더해 스턴 지속 시간을 결정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Stun", meta = (ClampMin = "0.0"))
	float StunDelayMin = 0.5f;

	/** 스턴 지속 추가 지연 최대값(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Stun", meta = (ClampMin = "0.0"))
	float StunDelayMax = 3.0f;

	// ── BP 설정용 — 무기 / 공격 데이터 ──────────────────────────────────

	/**
	 * BeginPlay에서 스폰해 손에 장착할 무기 클래스. BP 자식(BP_BWEnemy)에서 BP_<적무기>를 지정한다.
	 * null이면 맨손 상태로 시작한다. CombatComponent->DefaultEquipItemClass와 별개로 관리한다
	 * (적은 항상 손에 들고 시작하므로 BeginPlay에서 직접 손 소켓에 부착).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Weapon")
	TSubclassOf<ABWWeapon> DefaultWeaponClass;

	/**
	 * 랜덤 공격에 사용할 공격 DataTable. RowStruct = FBWAttackComboRow.
	 * 행(Light/Running/Special/Heavy)에서 랜덤 Step 1개를 선택해 재생한다.
	 * BP 자식에서 DT_<적>Attacks 에셋을 지정한다(경로 하드코딩 금지).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack")
	TObjectPtr<UDataTable> EnemyAttackDataTable;

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

	/**
	 * 현재 재생 중인 공격 몽타주. PerformAttack에서 설정, AbortTask에서 정지에 사용.
	 * GC 추적이 필요하므로 UPROPERTY + TObjectPtr.
	 */
	UPROPERTY()
	TObjectPtr<UAnimMontage> ActiveAttackMontage;

private:
	/**
	 * 적 무기 DataTable에서 ParriedHit 행 Steps[0].Montage를 조회한다.
	 * 무기/DataTable/행/Steps 없으면 nullptr 반환. GetBlockingHitMontage 패턴 동일.
	 */
	UAnimMontage* GetParriedHitMontage() const;

	/**
	 * Parried 몽타주 종료 콜백(안전망). EndParried를 호출해 Parried 상태를 확실히 해제한다.
	 * 타이머가 주경로이며 이 콜백은 보조 안전망이다.
	 */
	void OnParriedMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * Parried 상태 복구. bIsDead면 아무것도 하지 않는다.
	 * Parried 태그 제거 + 이동 복구(SetMovementMode) + 타이머 클리어.
	 */
	void EndParried();

	/**
	 * HitReaction 경로에서 호출되는 스턴 확률 적용 함수.
	 * StunnedRate% 확률로 Stunned 상태 진입 + 이동 차단 + 타이머 예약.
	 * @param HitReactionMontageLength 히트 리액션 몽타주 재생 길이(초).
	 */
	void TryApplyStun(float HitReactionMontageLength);

	/**
	 * Stunned 상태 복구. bIsDead 가드 후 Stunned 태그 제거 + 이동 복구 + 타이머 클리어.
	 */
	void EndStun();

	/** Parried 복구 타이머 핸들. EndPlay에서 ClearTimer. */
	FTimerHandle ParriedTimerHandle;

	/** Stunned 복구 타이머 핸들. EndPlay에서 ClearTimer. */
	FTimerHandle StunTimerHandle;
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

	/**
	 * BeginPlay에서 DefaultWeaponClass를 스폰해 손 소켓에 즉시 부착한다.
	 * CombatComponent에 위임해 RefreshCombatState로 AttackDataTable/콜리전을 자동 배선한다.
	 */
	void SpawnAndEquipDefaultWeapon();
};
