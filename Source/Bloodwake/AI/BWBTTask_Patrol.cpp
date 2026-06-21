// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BWBTTask_Patrol.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/TargetPoint.h"
#include "Character/BWEnemy.h"
#include "AI/BWAILog.h"

UBWBTTask_Patrol::UBWBTTask_Patrol()
{
	// BT 에디터에 표시될 노드 이름
	NodeName = TEXT("BW Patrol");

	// LocationKey가 Vector 타입 키만 보이도록 필터 등록
	LocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBWBTTask_Patrol, LocationKey));
}

EBTNodeResult::Type UBWBTTask_Patrol::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// ── 컨텍스트 획득 ──────────────────────────────────────────────────────────

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		UE_LOG(LogBWAI, Warning,
			TEXT("[BWBTTask_Patrol] AIController를 찾을 수 없습니다."));
		return EBTNodeResult::Failed;
	}

	ABWEnemy* Enemy = Cast<ABWEnemy>(AIController->GetPawn());
	if (!Enemy)
	{
		UE_LOG(LogBWAI, Warning,
			TEXT("[BWBTTask_Patrol] 제어 중인 폰이 ABWEnemy가 아닙니다. (%s)"),
			*AIController->GetName());
		return EBTNodeResult::Failed;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		return EBTNodeResult::Failed;
	}

	// ── PatrolPoints 유효성 검사 ──────────────────────────────────────────────

	const TArray<TObjectPtr<ATargetPoint>>& PatrolPoints = Enemy->GetPatrolPoints();
	if (PatrolPoints.Num() == 0)
	{
		// 순찰 지점이 없으면 즉시 Failed — BT는 다음 브랜치(대기 등)로 넘어간다.
		return EBTNodeResult::Failed;
	}

	// ── 인스턴스 메모리에서 현재 인덱스 획득 ─────────────────────────────────

	FBWPatrolTaskMemory* Memory = reinterpret_cast<FBWPatrolTaskMemory*>(NodeMemory);
	// GetInstanceMemorySize를 재정의했으므로 엔진 계약상 NodeMemory는 항상 유효하다.
	check(Memory != nullptr);

	// 인덱스가 배열 범위를 벗어나면 0으로 초기화 (에셋 변경 등 예외 대비)
	if (Memory->CurrentIndex >= PatrolPoints.Num())
	{
		Memory->CurrentIndex = 0;
	}

	// ── 현재 인덱스의 유효 지점 탐색 (null 슬롯 스킵) ────────────────────────

	ATargetPoint* NextPoint = nullptr;
	const int32 StartIndex = Memory->CurrentIndex;

	for (int32 i = 0; i < PatrolPoints.Num(); ++i)
	{
		const int32 CheckIndex = (StartIndex + i) % PatrolPoints.Num();
		if (IsValid(PatrolPoints[CheckIndex]))
		{
			NextPoint = PatrolPoints[CheckIndex].Get();
			// 다음 호출을 위해 인덱스를 순환 증가
			Memory->CurrentIndex = (CheckIndex + 1) % PatrolPoints.Num();
			break;
		}
	}

	if (!NextPoint)
	{
		// 모든 슬롯이 null인 경우
		UE_LOG(LogBWAI, Warning,
			TEXT("[BWBTTask_Patrol] PatrolPoints에 유효한 ATargetPoint가 없습니다. (%s)"),
			*Enemy->GetName());
		return EBTNodeResult::Failed;
	}

	// ── Blackboard Location 키에 목적지 기록 ─────────────────────────────────

	BB->SetValueAsVector(LocationKey.SelectedKeyName, NextPoint->GetActorLocation());

	return EBTNodeResult::Succeeded;
}

uint16 UBWBTTask_Patrol::GetInstanceMemorySize() const
{
	return sizeof(FBWPatrolTaskMemory);
}
