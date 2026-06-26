// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BWBTTask_PerformAttack.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"
#include "Character/BWEnemy.h"
#include "Character/BWStateComponent.h"
#include "Combat/BWCombatInterface.h"
#include "Core/BWGameplayDefine.h"
#include "AI/BWAILog.h"
#include "AI/BWEnemyAIController.h"

UBWBTTask_PerformAttack::UBWBTTask_PerformAttack()
{
	// BT 에디터에 표시될 노드 이름
	NodeName = TEXT("BW Perform Attack");

	// Latent 태스크 — InProgress 반환 후 FinishLatentTask로 완료를 통지한다.
	bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBWBTTask_PerformAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// ── 컨텍스트 획득 ──────────────────────────────────────────────────────────

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		UE_LOG(LogBWAI, Warning, TEXT("[BWBTTask_PerformAttack] AIController를 찾을 수 없습니다."));
		return EBTNodeResult::Failed;
	}

	ABWEnemy* Enemy = Cast<ABWEnemy>(AIController->GetPawn());
	if (!Enemy)
	{
		UE_LOG(LogBWAI, Warning, TEXT("[BWBTTask_PerformAttack] 제어 중인 폰이 ABWEnemy가 아닙니다. (%s)"),
			*AIController->GetName());
		return EBTNodeResult::Failed;
	}

	// IBWCombatInterface 유효성 확인
	IBWCombatInterface* CombatActor = Cast<IBWCombatInterface>(Enemy);
	if (!CombatActor)
	{
		UE_LOG(LogBWAI, Warning, TEXT("[BWBTTask_PerformAttack] ABWEnemy가 IBWCombatInterface를 구현하지 않습니다. (%s)"),
			*Enemy->GetName());
		return EBTNodeResult::Failed;
	}

	// ── 공격 직전 타깃 조준 회전 ──────────────────────────────────────────────
	// 적은 bOrientRotationToMovement=true(이동 방향으로만 회전)라, 사거리에서 멈춰 공격하면
	// 플레이어를 향하지 않아 헛스윙한다. 공격 커밋 직전 yaw를 타깃 쪽으로 즉시 정렬해
	// "플레이어를 바라본 상태로 공격"을 보장한다(소울라이크식 조준 후 커밋).
	if (ABWEnemyAIController* EnemyAI = Cast<ABWEnemyAIController>(AIController))
	{
		if (const AActor* Target = EnemyAI->GetCurrentTarget())
		{
			FVector ToTarget = Target->GetActorLocation() - Enemy->GetActorLocation();
			ToTarget.Z = 0.f;
			if (!ToTarget.IsNearlyZero())
			{
				FRotator FaceRotation = ToTarget.Rotation();
				FaceRotation.Pitch = 0.f;
				FaceRotation.Roll = 0.f;
				Enemy->SetActorRotation(FaceRotation);
			}
		}
	}

	// ── 완료 콜백 바인딩 ──────────────────────────────────────────────────────
	// FinishLatentTask는 UBTTaskNode(this)의 멤버이므로 태스크 노드 포인터를 캡처한다.
	// UBTTaskNode는 BT 에셋이 살아 있는 동안 유지된다 — Enemy보다 수명이 길다.
	// Enemy는 태스크 도중 사망할 수 있으므로 TWeakObjectPtr로 캡처해 IsValid 가드를 둔다.
	// OwnerComp도 수명 보장이 필요하므로 약참조로 캡처한다.
	UBWBTTask_PerformAttack* ThisTask = this;
	TWeakObjectPtr<ABWEnemy> WeakEnemy = Enemy;
	TWeakObjectPtr<UBehaviorTreeComponent> WeakOwnerComp = &OwnerComp;

	FOnMontageEnded OnEnded;
	OnEnded.BindLambda([ThisTask, WeakEnemy, WeakOwnerComp](UAnimMontage* /*Montage*/, bool /*bInterrupted*/)
	{
		// Enemy 수명 가드 — 몽타주 종료 시점에 Enemy가 이미 파괴됐을 수 있다.
		if (WeakEnemy.IsValid())
		{
			// 공격 상태 태그 제거 및 ActiveAttackMontage 클리어
			WeakEnemy->AbortCurrentAttack();
		}

		// BT Latent 태스크 완료 통지 — OwnerComp 수명 가드
		// Succeeded: 정상 완료든 중단(bInterrupted)이든 BT가 다음 행동으로 넘어갈 수 있도록 한다.
		if (WeakOwnerComp.IsValid() && ThisTask)
		{
			ThisTask->FinishAttack(*WeakOwnerComp.Get(), EBTNodeResult::Succeeded);
		}
	});

	// ── PerformAttack 호출 ────────────────────────────────────────────────────
	// OnEnded는 로컬 변수이므로 이동 시멘틱으로 전달한다(복사 비용 최소화).
	CombatActor->PerformAttack(MoveTemp(OnEnded));

	// Latent 태스크 — 몽타주 종료 콜백이 완료를 통지할 때까지 InProgress 상태 유지.
	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBWBTTask_PerformAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// BT가 이 태스크를 중단할 때 진행 중 공격 몽타주를 즉시 정지하고 상태를 초기화한다.
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		return EBTNodeResult::Aborted;
	}

	ABWEnemy* Enemy = Cast<ABWEnemy>(AIController->GetPawn());
	if (!IsValid(Enemy))
	{
		return EBTNodeResult::Aborted;
	}

	// AbortCurrentAttack: 진행 중 몽타주 정지 + 공격 상태 태그 초기화
	Enemy->AbortCurrentAttack();

	return EBTNodeResult::Aborted;
}
