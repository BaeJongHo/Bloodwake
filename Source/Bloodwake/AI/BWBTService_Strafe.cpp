// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BWBTService_Strafe.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Character/BWEnemy.h"
#include "AI/BWAILog.h"

UBWBTService_Strafe::UBWBTService_Strafe()
{
	NodeName = TEXT("BW Strafe");

	// Become/Cease/Tick 콜백 모두 활성 — 이동 속도·회전 설정을 브랜치 진입/이탈 시 정확히 1쌍으로 관리한다.
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant  = true;
	bNotifyTick           = true;

	// 새 랜덤 목적지 갱신 주기. BT 에디터 노드 디테일에서 조정 가능.
	// CLAUDE.md 4.1: 매 틱 이동 갱신 금지 — Interval 기반 주기 평가 사용.
	Interval        = 1.0f;
	RandomDeviation = 0.2f;

	// TargetKey: AActor 타입 Object 키만 선택 가능하도록 필터 등록
	TargetKey.AddObjectFilter(this,
		GET_MEMBER_NAME_CHECKED(UBWBTService_Strafe, TargetKey),
		AActor::StaticClass());
}

void UBWBTService_Strafe::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	FBWStrafeServiceMemory* Mem = reinterpret_cast<FBWStrafeServiceMemory*>(NodeMemory);
	check(Mem != nullptr);

	// ── 컨텍스트 획득 ──────────────────────────────────────────────────────────
	AAIController* AI = OwnerComp.GetAIOwner();
	if (!IsValid(AI))
	{
		Mem->bStateOverridden = 0;
		return;
	}

	ABWEnemy* Enemy = Cast<ABWEnemy>(AI->GetPawn());
	if (!IsValid(Enemy))
	{
		Mem->bStateOverridden = 0;
		return;
	}

	UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement();
	if (!MoveComp)
	{
		Mem->bStateOverridden = 0;
		return;
	}

	// ── 현재 값 백업 후 스트레이프 설정 적용 ────────────────────────────────────
	Mem->PrevMaxWalkSpeed              = MoveComp->MaxWalkSpeed;
	Mem->bPrevOrientRotationToMovement = MoveComp->bOrientRotationToMovement ? 1u : 0u;
	Mem->bPrevUseControllerRotationYaw = Enemy->bUseControllerRotationYaw    ? 1u : 0u;

	MoveComp->MaxWalkSpeed              = StrafeSpeed;
	MoveComp->bOrientRotationToMovement = false;  // 이동 방향으로 몸을 돌리지 않음
	Enemy->bUseControllerRotationYaw    = true;   // 컨트롤 회전(= Focus 방향)을 따라가 타깃 주시 달성

	Mem->bStateOverridden = 1;

	// ── 타깃 Focus 설정 ──────────────────────────────────────────────────────────
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (IsValid(BB))
	{
		if (AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName)))
		{
			AI->SetFocus(Target, EAIFocusPriority::Gameplay);
		}
	}
}

void UBWBTService_Strafe::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	// ── 컨텍스트 획득 ──────────────────────────────────────────────────────────
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

	// 공격·스턴·패링 중에는 이동 갱신을 보류한다.
	if (Enemy->IsAttacking() || Enemy->IsStunned() || Enemy->IsParried())
	{
		return;
	}

	// 타깃 Focus 재확인 (타깃 교체 대응 — idempotent)
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (IsValid(BB))
	{
		if (AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName)))
		{
			AI->SetFocus(Target, EAIFocusPriority::Gameplay);
		}
	}

	// ── 이동 중이면 새 목적지를 발행하지 않는다 (제자리 떨림 방지) ──────────────
	if (AI->GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		return;
	}

	// ── 랜덤 내비게이션 목적지 탐색 및 이동 지시 ───────────────────────────────
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy->GetWorld());
	if (!IsValid(NavSys))
	{
		UE_LOG(LogBWAI, Verbose,
			TEXT("[BWBTService_Strafe] NavigationSystem이 없습니다. 스트레이프 이동 스킵. (%s)"),
			*Enemy->GetName());
		return;
	}

	FNavLocation OutLocation;
	if (!NavSys->GetRandomReachablePointInRadius(Enemy->GetActorLocation(), StrafeRadius, OutLocation))
	{
		UE_LOG(LogBWAI, Verbose,
			TEXT("[BWBTService_Strafe] 반경 %.0f 내 도달 가능 지점 탐색 실패. (%s)"),
			StrafeRadius, *Enemy->GetName());
		return;
	}

	// bCanStrafe = true: 이동 중에도 Focus 회전을 유지해 옆걸음(스트레이프)이 성립한다.
	AI->MoveToLocation(
		OutLocation.Location,
		AcceptanceRadius,
		/*bStopOnOverlap=*/true,
		/*bUsePathfinding=*/true,
		/*bProjectDestinationToNavigation=*/false,
		/*bCanStrafe=*/true
	);
}

void UBWBTService_Strafe::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);

	FBWStrafeServiceMemory* Mem = reinterpret_cast<FBWStrafeServiceMemory*>(NodeMemory);
	check(Mem != nullptr);

	// ── 이동 속도·회전 설정 복원 ─────────────────────────────────────────────
	// bStateOverridden: OnBecomeRelevant에서 실제로 덮어썼을 때만 복원한다(미적용 시 기본값 덮어쓰기 방지).
	if (Mem->bStateOverridden)
	{
		AAIController* AI = OwnerComp.GetAIOwner();
		if (IsValid(AI))
		{
			ABWEnemy* Enemy = Cast<ABWEnemy>(AI->GetPawn());
			if (IsValid(Enemy))
			{
				UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement();
				if (MoveComp)
				{
					MoveComp->MaxWalkSpeed              = Mem->PrevMaxWalkSpeed;
					MoveComp->bOrientRotationToMovement = (Mem->bPrevOrientRotationToMovement != 0);
					Enemy->bUseControllerRotationYaw    = (Mem->bPrevUseControllerRotationYaw != 0);
				}
			}
		}
		Mem->bStateOverridden = 0;
	}

	// ── Focus 해제 + 진행 중 이동 취소 ──────────────────────────────────────────
	// 스태미나 회복 → Behavior 키 변경 → Selector 재탐색으로 브랜치 이탈 시,
	// 다음 브랜치(Approach/MeleeAttack)와 이동 요청이 경합하지 않도록 정리한다.
	AAIController* AI = OwnerComp.GetAIOwner();
	if (IsValid(AI))
	{
		AI->ClearFocus(EAIFocusPriority::Gameplay);
		AI->StopMovement();
	}
}

uint16 UBWBTService_Strafe::GetInstanceMemorySize() const
{
	return sizeof(FBWStrafeServiceMemory);
}
