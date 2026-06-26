// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "AI/BWAITypes.h"
#include "BWBTService_SelectBehavior.generated.h"

class UBlackboardComponent;

/**
 * 적 AI 행동 패턴을 매 주기 평가해 Blackboard Behavior 키를 갱신하는 BT Service.
 * 블랙보드 Target 키(SSOT)와 순찰점 유무, 공격 사거리를 기반으로
 * EBWAIBehavior(Idle/Patrol/Approach/MeleeAttack)를 결정한다.
 *
 * 배치: BT_Enemy 루트 Selector에 Service로 추가해 항상 평가되도록 한다.
 * 설정: BehaviorKey = Behavior, TargetKey = Target, AttackRangeDistance = 원하는 사거리.
 *
 * 1단계: GAS 미적용, 싱글플레이 전용, Tick은 Service Interval로 대체(CLAUDE.md 4.1).
 */
UCLASS()
class BLOODWAKE_API UBWBTService_SelectBehavior : public UBTService
{
	GENERATED_BODY()

public:
	UBWBTService_SelectBehavior();

protected:
	/**
	 * BT Service 주기 평가 진입점.
	 * Interval마다 호출되며 UpdateBehavior를 실행한다.
	 */
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
	/**
	 * 타깃 유무·거리·순찰점을 평가해 EBWAIBehavior를 결정하고
	 * SetBehaviorKey로 블랙보드에 기록한다.
	 */
	void UpdateBehavior(UBehaviorTreeComponent& OwnerComp);

	/**
	 * 블랙보드 BehaviorKey에 지정된 EBWAIBehavior 값을 SetValueAsEnum으로 기록한다.
	 */
	void SetBehaviorKey(UBlackboardComponent* Comp, EBWAIBehavior Behavior);

	// ── BT 노드 디테일 패널 설정값 ─────────────────────────────────────────────

	/**
	 * 이 거리(cm) 이내면 MeleeAttack, 밖이면 Approach로 분기한다.
	 * BT 에디터 노드 디테일 패널에서 인스턴스별로 튜닝한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Behavior", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float AttackRangeDistance = 200.f;

	/**
	 * 쓸 Enum 키 선택자. BT 에디터에서 BB_Enemy의 Behavior 키를 지정한다.
	 * 생성자에서 EBWAIBehavior 타입 필터를 등록해 Enum 키만 표시한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Blackboard", meta = (AllowPrivateAccess = "true"))
	FBlackboardKeySelector BehaviorKey;

	/**
	 * 타깃 감지 여부를 읽을 Object 키 선택자. BB_Enemy의 Target 키를 지정한다.
	 * 생성자에서 AActor 타입 필터를 등록한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Blackboard", meta = (AllowPrivateAccess = "true"))
	FBlackboardKeySelector TargetKey;
};
