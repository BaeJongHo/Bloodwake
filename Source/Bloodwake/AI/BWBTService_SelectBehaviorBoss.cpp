// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BWBTService_SelectBehaviorBoss.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/BWEnemy.h"
#include "Combat/BWAttributeComponent.h"

UBWBTService_SelectBehaviorBoss::UBWBTService_SelectBehaviorBoss()
{
	// BT 에디터에 표시될 노드 이름 — 일반 Select Behavior와 구분한다.
	NodeName = TEXT("BW Select Behavior (Boss)");
}

void UBWBTService_SelectBehaviorBoss::UpdateBehavior(UBehaviorTreeComponent& OwnerComp)
{
	// ── 컨텍스트 획득 (부모 UpdateBehavior와 동일 패턴) ────────────────────────────
	AAIController* AI = OwnerComp.GetAIOwner();
	if (!IsValid(AI))
	{
		return;
	}

	ABWEnemy* Enemy = Cast<ABWEnemy>(AI->GetPawn());
	if (!IsValid(Enemy))
	{
		return;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!IsValid(BB))
	{
		return;
	}

	// ── 스턴/패링 — 보스는 Stunned 브랜치가 없으므로 Idle로 정지 ────────────────────
	// 부모는 여기서 Stunned를 쓰지만, 보스 BT엔 Stunned 브랜치가 없어 Selector가 멈춘다.
	// 대신 Idle을 써서 진행 중 공격을 캔슬(MeleeAttack 데코레이터 Self-abort)하고
	// 제자리에서 대기시킨다(소울라이크식 패링→경직 정지).
	if (Enemy->IsParried() || Enemy->IsStunned())
	{
		SetBehaviorKey(BB, EBWAIBehavior::Idle);
		return;
	}

	// ── 공격 모션 중 행동 고정(hold) ──────────────────────────────────────────────
	// 공격 재생 중에는 사거리 밖으로 벗어나도 MeleeAttack을 유지해 진행 중 PerformAttack
	// 태스크가 Abort(모션 캔슬)되지 않게 한다. 공격 종료 시 아래 일반 로직으로 재평가된다.
	if (Enemy->IsAttacking())
	{
		SetBehaviorKey(BB, EBWAIBehavior::MeleeAttack);
		return;
	}

	// ── 타깃 감지 여부 — 블랙보드 Target 키 SSOT ─────────────────────────────────
	// ABWEnemyAIController::UpdateTarget이 Sight 기반으로 관리하는 Target 키를 직접 읽는다.
	UObject* TargetObject = BB->GetValueAsObject(TargetKey.SelectedKeyName);
	AActor* TargetActor = Cast<AActor>(TargetObject);

	// ── 타깃 없음 → Idle (보스는 순찰하지 않는다) ─────────────────────────────────
	if (TargetActor == nullptr)
	{
		SetBehaviorKey(BB, EBWAIBehavior::Idle);
		return;
	}

	// ── [신규] 스태미나 부족 → Strafe (간보기) ───────────────────────────────────
	// IsAttacking() 분기(2번) 뒤에 위치해야 한다.
	// 공격 시작 시 스태미나를 소비하므로, 판정이 앞서면 자기 공격 모션을 자기가 캔슬하는 문제가 발생한다.
	const UBWAttributeComponent* Attr = Enemy->GetAttributeComponent();
	if (IsValid(Attr) && !Attr->IsStaminaAboveThreshold(StrafeStaminaThreshold))
	{
		SetBehaviorKey(BB, EBWAIBehavior::Strafe);
		return;
	}

	// ── 타깃 있음 → 사거리 기준 MeleeAttack / Approach ────────────────────────────
	const float DistSq = FVector::DistSquared(Enemy->GetActorLocation(), TargetActor->GetActorLocation());
	const EBWAIBehavior Behavior = (DistSq <= FMath::Square(AttackRangeDistance))
		? EBWAIBehavior::MeleeAttack
		: EBWAIBehavior::Approach;
	SetBehaviorKey(BB, Behavior);
}
