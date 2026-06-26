// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/BWAILog.h"
#include "BWEnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Damage;
class UBehaviorTree;
class ABWEnemy;
struct FAIStimulus;

/**
 * 적 캐릭터(ABWEnemy)를 위한 AI Controller.
 * AIPerception(Sight)으로 플레이어를 인지하고 Blackboard의 "Target" 키에 기록한다.
 * BehaviorTree를 실행하여 순찰↔추격 전환을 BT Decorator로 분기한다.
 * 싱글플레이 전용 — 리플리케이션 코드 없음.
 */
UCLASS(Blueprintable)
class BLOODWAKE_API ABWEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	ABWEnemyAIController();

	/**
	 * 현재 추격 중인 타깃(블랙보드 Target 키 값)을 반환한다. 없으면 nullptr.
	 * UBWBTTask_PerformAttack이 공격 직전 조준 회전 대상으로 사용한다.
	 */
	AActor* GetCurrentTarget() const;

protected:
	/** Pawn 빙의 시 BT 실행 + Perception 델리게이트 바인딩 + UpdateTarget 타이머 시작. */
	virtual void OnPossess(APawn* InPawn) override;

	/** 빙의 해제 시 타이머·델리게이트 정리 + BT 정지. */
	virtual void OnUnPossess() override;

	/**
	 * BP 아키타입 프로퍼티 오버라이드가 적용된 뒤(BeginPlay 시점) SightConfig에 수치를 재적용한다.
	 * 생성자에서 한 번, BeginPlay에서 다시 호출해 BP 클래스 디폴트 튜닝값이 실제 Perception에 반영되도록 한다.
	 */
	void ApplySightConfig();

	/** BeginPlay에서 ApplySightConfig를 다시 호출해 BP 디폴트 오버라이드를 Perception에 반영한다. */
	virtual void BeginPlay() override;

	/** 빙의한 Pawn(ACharacter)의 MaxWalkSpeed를 NewSpeed로 설정한다. 순찰/추격 전환 시 호출. */
	void ApplyMovementSpeed(float NewSpeed);

	/**
	 * AIPerception이 현재 인지 중인 액터들에서 ABWPlayerCharacter를 찾아
	 * Blackboard "Target" 키에 설정한다. 없으면 ClearValue로 해제.
	 * OnPossess에서 TargetUpdateInterval 주기 타이머로 반복 호출된다.
	 */
	void UpdateTarget();

	// ── OnTargetPerceptionUpdated 콜백 ──────────────────────────────────────

	/**
	 * AIPerception의 OnTargetPerceptionUpdated 델리게이트 콜백.
	 * 인지 이벤트 발생 즉시 UpdateTarget을 호출해 즉각 반응한다(타이머 보조).
	 * AddDynamic 바인딩 대상이므로 UFUNCTION 필수.
	 */
	UFUNCTION()
	void HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	// ── 컴포넌트 ──────────────────────────────────────────────────────────────

	/** 시야 기반 인지 컴포넌트. 생성자에서 CreateDefaultSubobject. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAIPerceptionComponent> AIPerception;

	/** Sight 감각 설정. 생성자에서 생성해 AIPerception에 ConfigureSense. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/**
	 * Damage 감각 설정. 생성자에서 생성해 AIPerception에 ConfigureSense.
	 * TakeDamage에서 UAISense_Damage::ReportDamageEvent 호출 시 이 감각이 자극을 수신한다.
	 * 시야 밖 피격 보조 역할 — 타깃 결정 SSOT는 Target 블랙보드 키 유지.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;

	// ── BP 설정용 데이터 ──────────────────────────────────────────────────────

	/** 이 적이 실행할 BehaviorTree. BP 자식(BP_EnemyAIController)에서 BT_Enemy 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	/** UpdateTarget 폴링 주기(초). 기본 0.1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0.02"))
	float TargetUpdateInterval = 0.1f;

	/** 시야 반경(cm). 생성자에서 SightConfig에 적용. BP에서 튜닝 가능. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Perception", meta = (ClampMin = "0.0"))
	float SightRadius = 1500.f;

	/** 시야 상실 반경(cm). SightRadius보다 크게 설정해야 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Perception", meta = (ClampMin = "0.0"))
	float LoseSightRadius = 1800.f;

	/** 시야 반각(도). PeripheralVisionAngleDegrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Perception", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float VisionAngleDegrees = 60.f;

	/** 시야에서 사라진 뒤 자극 유지 시간(초). MaxAge. 0이면 무제한. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Perception", meta = (ClampMin = "0.0"))
	float SightMaxAge = 5.f;

	/**
	 * 시야/피격으로 잃은 뒤에도 타깃을 추격으로 "기억"하는 시간(초).
	 * 0.1초 폴링(UpdateTarget)이 피격으로 막 잡은 타깃을 즉시 지우는 것을 방지한다.
	 * 시야로 다시 보거나 추가 피격이 들어오면 갱신된다. 0이면 즉시 망각.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Perception", meta = (ClampMin = "0.0"))
	float TargetMemoryDuration = 3.f;

	// ── 이동 속도(순찰/추격 분리) ───────────────────────────────────────────────

	/** 순찰 중 이동 속도(cm/s). 추격보다 느리게 둔다. BP에서 튜닝. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float PatrolSpeed = 150.f;

	/** 추격(Target 인지) 중 이동 속도(cm/s). BP에서 튜닝. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float ChaseSpeed = 350.f;

	// ── Blackboard 키 이름(코드↔에셋 계약) ────────────────────────────────────

	/**
	 * Blackboard의 타겟 키 이름. BB 에셋의 Object 키 이름과 반드시 일치해야 한다.
	 * 기본값 "Target". BP 자식에서 재지정 가능.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Blackboard")
	FName TargetBlackboardKey = TEXT("Target");

private:
	/** UpdateTarget 반복 타이머 핸들. OnUnPossess에서 ClearTimer. 비-UObject이므로 UPROPERTY 불필요. */
	FTimerHandle TargetTimerHandle;

	/** 현재 추격 중인지 여부. 순찰↔추격 전환 시에만 이동 속도를 바꾸기 위한 상태 캐시. */
	bool bIsChasing = false;

	/**
	 * 시야 또는 피격으로 잡은 "기억 타깃"의 약참조. UpdateTarget이 단일 소유(SSOT)한다.
	 * 시야 known-list가 비어도 ForgetTargetTime 전까지는 이 타깃으로 추격을 유지해,
	 * 0.1초 폴링이 피격 타깃을 즉시 지우는 플리커를 막는다.
	 */
	TWeakObjectPtr<AActor> RememberedTarget;

	/** RememberedTarget을 잊는 월드 시각(초). GetWorld()->GetTimeSeconds() 기준. */
	float ForgetTargetTime = 0.f;

	/**
	 * 빙의 중인 ABWEnemy의 약참조. OnPossess에서 캐시, OnUnPossess에서 무효화.
	 * 소유하지 않으므로 TWeakObjectPtr 사용(CLAUDE.md 3.3).
	 * UpdateTarget에서 매 호출 Cast를 피하기 위한 캐시 (CLAUDE.md 4.1).
	 */
	TWeakObjectPtr<ABWEnemy> CachedEnemy;
};
