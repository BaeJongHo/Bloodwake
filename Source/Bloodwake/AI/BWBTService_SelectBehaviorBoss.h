// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AI/BWBTService_SelectBehavior.h"
#include "BWBTService_SelectBehaviorBoss.generated.h"

class UBehaviorTreeComponent;

/**
 * 보스 전용 BT 행동 결정 서비스. UBWBTService_SelectBehavior를 상속한다.
 *
 * 보스 BT(BT_Boss)는 Idle / Approach / MeleeAttack 세 브랜치만 가진다 — Patrol · Stunned 브랜치가 없다.
 * 그래서 부모의 DecideBehavior만 교체하는 방식은 위험하다: 부모 UpdateBehavior가 스턴/패링 시
 * Stunned를, (파생이 막지 않으면) 순찰점 유무로 Patrol을 쓸 수 있고, 보스 BT엔 그 브랜치가 없어
 * Selector가 매칭 실패로 멈춘다. 이를 원천 차단하기 위해 UpdateBehavior 자체를 override해
 * 보스가 사용할 수 있는 값(Idle/Approach/MeleeAttack)만 쓰도록 재구성한다:
 *   - 스턴/패링 중        → Idle (진행 중 공격 캔슬 + 제자리 정지. Stunned 값 미사용)
 *   - 공격 모션 재생 중    → MeleeAttack 유지 (사거리 밖으로 벗어나도 모션 캔슬 방지)
 *   - 타깃 없음           → Idle (순찰 미사용)
 *   - 타깃 있음 + 사거리 내 → MeleeAttack
 *   - 타깃 있음 + 사거리 밖 → Approach
 *
 * AttackRangeDistance / BehaviorKey / TargetKey는 BT 노드 디테일 패널에서 설정한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS()
class BLOODWAKE_API UBWBTService_SelectBehaviorBoss : public UBWBTService_SelectBehavior
{
	GENERATED_BODY()

public:
	UBWBTService_SelectBehaviorBoss();

protected:
	/**
	 * 보스 전용 행동 평가 override. 부모의 결정 로직을 대체하며,
	 * 보스 BT가 가진 Idle/Approach/MeleeAttack/Strafe 값만 블랙보드에 기록한다.
	 * (부모의 DecideBehavior는 호출하지 않는다 — Patrol/Stunned 유입 원천 차단.)
	 *
	 * 결정 순서:
	 *   1. 패링/스턴 → Idle
	 *   2. 공격 모션 재생 중 → MeleeAttack 유지 (모션 캔슬 방지)
	 *   3. 타깃 없음 → Idle
	 *   4. 스태미나 부족 → Strafe (공격 홀드 분기 뒤 — 자기 공격이 스태미나 임계로 캔슬되는 것 방지)
	 *   5. 거리 기준 → MeleeAttack / Approach
	 */
	virtual void UpdateBehavior(UBehaviorTreeComponent& OwnerComp) override;

	/**
	 * 이 값 미만이면 공격을 중단하고 Strafe(간보기)로 전환한다. 절대값(스태미나 포인트) 기준.
	 * BT 노드 디테일 패널에서 튜닝한다. UBWAttributeComponent::IsStaminaAboveThreshold 재사용.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Behavior", meta = (ClampMin = "0.0"))
	float StrafeStaminaThreshold = 30.f;
};
