// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Combat/BWAttackTypes.h"
#include "BWBTTask_PerformAttack.generated.h"

/**
 * ABWEnemy의 IBWCombatInterface::PerformAttack을 호출하는 BT Latent 태스크.
 * ExecuteTask에서 공격을 시작하고 InProgress를 반환한다.
 * 몽타주 종료 시 BindLambda 콜백이 FinishLatentTask(Succeeded)를 호출해 태스크를 완료한다.
 * AbortTask에서 진행 중 몽타주를 정지하고 상태를 초기화하는 안전망을 제공한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS()
class BLOODWAKE_API UBWBTTask_PerformAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBWBTTask_PerformAttack();

	/** 공격을 시작하고 InProgress를 반환한다. 몽타주 종료 콜백이 FinishLatentTask를 호출한다. */
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	/**
	 * BT가 태스크를 중단할 때(예: 더 높은 우선순위 브랜치 활성화) 호출된다.
	 * 진행 중 공격 몽타주를 정지하고 공격 상태 태그를 초기화한다.
	 */
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	/**
	 * FinishLatentTask 래퍼. FinishLatentTask는 protected 멤버이므로,
	 * 람다 캡처에서 this 포인터를 통해 호출하기 위한 public 진입점을 제공한다.
	 */
	void FinishAttack(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type Result)
	{
		FinishLatentTask(OwnerComp, Result);
	}

protected:
	/**
	 * 이 노드가 재생할 공격 종류. BT 에디터 노드 디테일에서 지정한다(Light/Heavy/Special 등).
	 * ABWEnemy::PerformAttack이 이 타입에 해당하는 DataTable 행에서 몽타주를 선택한다.
	 * 같은 BW Perform Attack 태스크를 타입만 다르게 여러 노드로 배치해 패턴을 구성할 수 있다.
	 */
	UPROPERTY(EditAnywhere, Category = "Attack")
	EBWAttackType AttackType = EBWAttackType::Light;

	/**
	 * 공격 진입 직전 타깃이 없으면 이 키에 EBWAIBehavior::Idle을 기록한다(헛공격 방지 방어 로직).
	 * BT 노드 디테일 패널에서 BB_Enemy의 Behavior 키를 지정한다.
	 * 생성자에서 EBWAIBehavior 타입 필터를 등록해 Enum 키만 표시한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector BehaviorKey;
};
