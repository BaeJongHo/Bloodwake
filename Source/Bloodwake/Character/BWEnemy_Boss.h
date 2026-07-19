// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/BWEnemy.h"
#include "BWEnemy_Boss.generated.h"

class UBWBossHealthBarWidget;
class USoundBase;

/**
 * 소울라이크 보스 캐릭터 C++ 베이스 클래스. ABWEnemy를 상속한다.
 *
 * 주요 동작:
 *  - TwoHanded 무기 장착은 부모 SpawnAndEquipDefaultWeapon 경로 재사용(DefaultWeaponClass 지정).
 *  - 오버헤드 HP 바(UBWEnemyHealthBarComponent)를 BeginPlay에서 제거하고
 *    AddToViewport 기반 2D 스크린 바(UBWBossHealthBarWidget)로 교체한다.
 *  - 사망 시(EnableRagdoll) 장착 무기를 detach + 물리 시뮬레이션으로 떨궈 바닥에 낙하시킨다.
 *  - ShouldFaceTargetBeforeAttack을 false로 재정의해 BWBTTask_PerformAttack의 즉시 스냅 회전을
 *    비활성화한다(RotateToTarget 노티파이가 windup 동안 부드러운 회전을 담당).
 *
 * BP 자식(BP_Boss) 설정 항목: SkeletalMesh/ABP, DefaultWeaponClass, EnemyAttackDataTable,
 * BossHealthBarWidgetClass=WBP_BossHealthBar, BossName, DroppedWeaponCollisionProfile/LifeSpan,
 * AttributeComponent(상속됨) MaxHealth 등. 상세는 설계 문서 참조.
 *
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS(Blueprintable)
class BLOODWAKE_API ABWEnemy_Boss : public ABWEnemy
{
	GENERATED_BODY()

public:
	ABWEnemy_Boss();

protected:
	/**
	 * Super::BeginPlay() 후 오버헤드 HP 바 컴포넌트를 파괴해 틱·렌더 비용을 제거한다.
	 * Super 이후 파괴해야 컴포넌트 EndPlay에서 Attribute 델리게이트가 안전하게 해제된다.
	 */
	virtual void BeginPlay() override;

	/**
	 * 2D 보스 체력바를 뷰포트에서 제거한다.
	 * NativeDestruct가 Attribute 구독 해제를 담당한다.
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ── ABWEnemy 오버라이드 ──────────────────────────────────────────────────

	/** 오버헤드 HP 바 대신 2D 보스 체력바를 노출한다. ABWEnemyAIController가 추격 전환 시 호출. */
	virtual void ShowHealthBar() override;

	/** 2D 보스 체력바를 Collapsed로 숨긴다. 시야를 놓쳤을 때 호출. */
	virtual void HideHealthBar() override;

	/**
	 * 무기 드롭 + 2D 체력바 제거 후 Super::EnableRagdoll()을 호출한다.
	 * Super 호출로 bIsDead·Death 태그·랙돌·CorpseLifeSpan을 그대로 처리한다.
	 */
	virtual void EnableRagdoll() override;

	/**
	 * 항상 false 반환 — BWBTTask_PerformAttack의 즉시 스냅 회전을 비활성화한다.
	 * RotateToTarget AnimNotifyState가 windup 동안 부드럽게 회전을 담당한다.
	 */
	virtual bool ShouldFaceTargetBeforeAttack() const override { return false; }

private:
	/** 장착된 무기를 detach 후 물리 시뮬레이션으로 바닥에 낙하시킨다. */
	void DropEquippedWeapon();

	/**
	 * UBWBossMusicSubsystem에 BGM 재생을 요청한다.
	 * BossBGM이 null이거나 서브시스템을 찾을 수 없으면 경고 후 무동작.
	 * ShowHealthBar() 말미에서 호출된다.
	 */
	void StartBossBGM();

	/**
	 * UBWBossMusicSubsystem에 BGM 정지를 요청한다.
	 * 이미 정지 상태이거나 서브시스템을 찾을 수 없으면 조용히 무시.
	 * HideHealthBar(), EnableRagdoll(), EndPlay()에서 호출된다.
	 */
	void StopBossBGM();

	/**
	 * 최초 노출 시 BossHealthBarWidgetClass로 위젯 인스턴스를 생성하고
	 * AddToViewport + InitializeForBoss를 수행한다. 이미 유효하면 즉시 반환.
	 */
	void EnsureBossHealthBar();

	// ── BP 설정용 — 보스 BGM ─────────────────────────────────────────────────

	/**
	 * 전투 돌입 시 재생할 보스 BGM 에셋.
	 * BP 자식(BP_Boss)에서 SC_ 또는 SW_ 에셋을 지정한다. null이면 BGM 재생 없음.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Audio", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USoundBase> BossBGM;

	/** BGM 페이드인 시간(초). 기본 1.0초. BP 자식에서 조정 가능. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Audio", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float BGMFadeInDuration = 1.0f;

	/** BGM 페이드아웃 시간(초). 기본 1.5초. BP 자식에서 조정 가능. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Audio", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float BGMFadeOutDuration = 1.5f;

	// ── BP 설정용 — 2D 보스 체력바 ───────────────────────────────────────────

	/**
	 * 화면 상단 2D 보스 HP 바 위젯 클래스.
	 * BP 자식(BP_Boss)에서 WBP_BossHealthBar 에셋을 지정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UBWBossHealthBarWidget> BossHealthBarWidgetClass;

	/**
	 * 체력바에 표시할 보스 이름 텍스트.
	 * BP 자식(BP_Boss)의 디테일 패널에서 설정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|UI", meta = (AllowPrivateAccess = "true"))
	FText BossName;

	// ── BP 설정용 — 무기 드롭 ────────────────────────────────────────────────

	/**
	 * 드롭된 무기 메시에 적용할 충돌 프로파일명. 기본 "PhysicsActor".
	 * SM 에셋에 Simple Collision(바디 셋업)이 있어야 물리가 정상 동작한다(에셋 작업 필요).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Death", meta = (AllowPrivateAccess = "true"))
	FName DroppedWeaponCollisionProfile = TEXT("PhysicsActor");

	/**
	 * 드롭된 무기 잔존 시간(초). 0이면 영구 잔존.
	 * 보스 시체(CorpseLifeSpan)와 독립적으로 무기가 잔존한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Death",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DroppedWeaponLifeSpan = 0.f;

	// ── 런타임 ───────────────────────────────────────────────────────────────

	/**
	 * 생성된 2D 보스 HP 바 위젯 인스턴스. EnsureBossHealthBar가 최초 노출 시 생성한다.
	 * GC 추적을 위해 UPROPERTY(Transient) + TObjectPtr.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UBWBossHealthBarWidget> BossHealthBarInstance;
};
