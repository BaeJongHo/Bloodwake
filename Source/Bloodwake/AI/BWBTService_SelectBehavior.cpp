// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BWBTService_SelectBehavior.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/BWEnemy.h"
#include "AI/BWAILog.h"

UBWBTService_SelectBehavior::UBWBTService_SelectBehavior()
{
	// BT 에디터에 표시될 노드 이름.
	NodeName = TEXT("BW Select Behavior");

	// TickNode 호출이 보장되도록 명시한다.
	bNotifyTick = true;

	// 기본 평가 주기(초). BT 에디터 노드 디테일에서 인스턴스별 조정 가능.
	Interval = 0.2f;
	RandomDeviation = 0.05f;

	// ── 블랙보드 키 필터 등록 ────────────────────────────────────────────────
	// BehaviorKey: EBWAIBehavior 타입 Enum 키만 선택 가능하도록 필터를 건다.
	BehaviorKey.AddEnumFilter(
		this,
		GET_MEMBER_NAME_CHECKED(UBWBTService_SelectBehavior, BehaviorKey),
		StaticEnum<EBWAIBehavior>());

	// TargetKey: AActor(및 파생) Object 키만 선택 가능하도록 필터를 건다.
	TargetKey.AddObjectFilter(
		this,
		GET_MEMBER_NAME_CHECKED(UBWBTService_SelectBehavior, TargetKey),
		AActor::StaticClass());
}

void UBWBTService_SelectBehavior::TickNode(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	UpdateBehavior(OwnerComp);
}

void UBWBTService_SelectBehavior::UpdateBehavior(UBehaviorTreeComponent& OwnerComp)
{
	// ── 컨텍스트 획득 ─────────────────────────────────────────────────────────

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

	// ── 타깃 감지 여부 — 블랙보드 Target 키 SSOT ─────────────────────────────
	// ABWEnemyAIController::UpdateTarget이 Sight known-list 기반으로 Target 키를 관리하는 SSOT.
	// Service가 Perception을 다시 읽으면 로직이 이중화되므로 블랙보드 값을 직접 읽는다.
	UObject* TargetObject = BB->GetValueAsObject(TargetKey.SelectedKeyName);
	AActor* TargetActor = Cast<AActor>(TargetObject);

	// ── 순찰점 존재 여부 — BWBTTask_Patrol과 동일 기준(Num() > 0) ───────────────
	const bool bHasPatrol = Enemy->GetPatrolPoints().Num() > 0;

	// ── 행동 결정 분기 ────────────────────────────────────────────────────────
	EBWAIBehavior Behavior = EBWAIBehavior::Idle;

	if (TargetActor == nullptr)
	{
		// 타깃 없음 — 순찰점 유무로 Idle/Patrol 분기
		Behavior = bHasPatrol ? EBWAIBehavior::Patrol : EBWAIBehavior::Idle;
	}
	else
	{
		// 타깃 있음 — 거리 비교로 Approach/MeleeAttack 분기
		const float DistSq = FVector::DistSquared(
			Enemy->GetActorLocation(), TargetActor->GetActorLocation());
		const bool bInRange = DistSq <= FMath::Square(AttackRangeDistance);
		Behavior = bInRange ? EBWAIBehavior::MeleeAttack : EBWAIBehavior::Approach;
	}

	SetBehaviorKey(BB, Behavior);
}

void UBWBTService_SelectBehavior::SetBehaviorKey(UBlackboardComponent* Comp, EBWAIBehavior Behavior)
{
	if (!IsValid(Comp))
	{
		return;
	}

	Comp->SetValueAsEnum(BehaviorKey.SelectedKeyName, static_cast<uint8>(Behavior));
}
