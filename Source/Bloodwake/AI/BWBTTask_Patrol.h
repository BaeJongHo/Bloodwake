// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BWBTTask_Patrol.generated.h"

/**
 * ABWEnemy의 PatrolPoints를 순환 인덱스로 순회하며
 * 다음 순찰 목적지를 Blackboard Location(Vector) 키에 기록한다.
 * 후속 MoveTo Task가 해당 위치로 폰을 이동시킨다.
 *
 * 인덱스는 BT 노드 인스턴스 메모리(NodeMemory)에 보관해
 * 각 BT 인스턴스가 독립적인 순찰 인덱스를 소유한다.
 */
UCLASS()
class BLOODWAKE_API UBWBTTask_Patrol : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBWBTTask_Patrol();

	/** 순찰 인덱스를 증가시키고 다음 지점을 Blackboard Location 키에 기록. */
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	/** 인스턴스 메모리 크기(FBWPatrolTaskMemory) 반환. 반드시 구현해야 NodeMemory 캐스팅이 안전하다. */
	virtual uint16 GetInstanceMemorySize() const override;

	/**
	 * 다음 순찰 위치를 쓸 Blackboard 키(Vector 타입).
	 * BT 에디터의 노드 프로퍼티에서 키를 선택한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector LocationKey;
};

/**
 * Patrol Task 인스턴스별 메모리.
 * 현재 순찰 인덱스를 BT 노드 메모리에 보관해 각 적이 독립적인 순찰 진행 상태를 유지한다.
 */
struct FBWPatrolTaskMemory
{
	int32 CurrentIndex = 0;
};
