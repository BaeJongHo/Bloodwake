// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BWBTService_Strafe.generated.h"

class AAIController;
class ABWEnemy;
class UCharacterMovementComponent;

/**
 * BT_Boss의 Strafe 브랜치가 활성인 동안 보스가 플레이어를 주시(Focus)하며
 * 현재 위치 반경 내 랜덤 내비게이션 지점으로 저속 이동(간보기)하는 BT Service.
 *
 * OnBecomeRelevant: 이동 속도·회전 설정을 스트레이프 전용으로 전환하고 타깃 Focus를 설정한다.
 * TickNode        : BT Service Interval마다 Idle 상태일 때만 새 랜덤 목적지를 MoveToLocation으로 지시한다.
 * OnCeaseRelevant: 이동 속도·회전 설정을 진입 시점 값으로 복원하고 Focus와 이동 요청을 정리한다.
 *
 * 런타임 상태는 멤버 변수가 아닌 BT 노드 인스턴스 메모리(NodeMemory)에 보관한다.
 * BT 에셋 단위로 노드 객체가 공유되므로, 멤버에 두면 보스가 2체 이상일 때 상태가 오염된다.
 * 기존 UBWBTTask_Patrol(FBWPatrolTaskMemory)과 동일 패턴.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS()
class BLOODWAKE_API UBWBTService_Strafe : public UBTService
{
	GENERATED_BODY()

public:
	UBWBTService_Strafe();

protected:
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	/** 인스턴스 메모리 크기. 반드시 재정의해야 NodeMemory 캐스팅이 안전하다. */
	virtual uint16 GetInstanceMemorySize() const override;

	// ── BT 노드 디테일 패널 설정값 ──────────────────────────────────────────

	/**
	 * 현재 위치 기준 랜덤 이동 목적지 반경(cm).
	 * NavigationSystem::GetRandomReachablePointInRadius의 반경으로 사용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Strafe", meta = (ClampMin = "0.0"))
	float StrafeRadius = 400.f;

	/** 스트레이프 중 MaxWalkSpeed. OnBecomeRelevant에서 적용, OnCeaseRelevant에서 복원된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Strafe", meta = (ClampMin = "0.0"))
	float StrafeSpeed = 200.f;

	/** MoveToLocation 도착 허용 반경(cm). 이 범위 안에 들어오면 도착으로 판정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Strafe", meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 50.f;

	/**
	 * 타깃(주시 대상) Blackboard 키 선택자. AActor 타입으로 필터 등록.
	 * BT 에디터에서 BB_Boss의 Target 키를 선택한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetKey;
};

/**
 * Strafe 서비스 인스턴스별 노드 메모리.
 * 진입 시점의 이동 속도·회전 설정을 보관해 OnCeaseRelevant에서 정확히 복원한다.
 * bStateOverridden: 실제로 설정을 덮어썼을 때만 true로 설정해 미적용 시 복원을 스킵한다.
 */
struct FBWStrafeServiceMemory
{
	float PrevMaxWalkSpeed;
	uint8 bPrevOrientRotationToMovement : 1;
	uint8 bPrevUseControllerRotationYaw : 1;
	uint8 bStateOverridden : 1;

	FBWStrafeServiceMemory()
		: PrevMaxWalkSpeed(0.f)
		, bPrevOrientRotationToMovement(0)
		, bPrevUseControllerRotationYaw(0)
		, bStateOverridden(0)
	{}
};
