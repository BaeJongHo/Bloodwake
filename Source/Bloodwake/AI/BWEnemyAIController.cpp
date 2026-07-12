// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BWEnemyAIController.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/BWPlayerCharacter.h"
#include "Character/BWEnemy.h"

ABWEnemyAIController::ABWEnemyAIController()
{
	// AI Controller는 틱이 필요 없다 — 인지는 이벤트 기반, 갱신은 타이머 기반.
	PrimaryActorTick.bCanEverTick = false;

	// ── AIPerceptionComponent 구성 ──────────────────────────────────────────
	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));

	// ── SightConfig 구성 ───────────────────────────────────────────────────
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	// 소속 필터 — 플레이어 1인만 감지하면 충분하므로 모든 소속 감지 활성화.
	// UpdateTarget에서 ABWPlayerCharacter 캐스팅으로 실제 필터링한다.
	SightConfig->DetectionByAffiliation.bDetectEnemies    = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals   = true;

	// 1차 적용: 생성자 기본값으로 SightConfig를 초기화.
	// BP 아키타입 오버라이드는 아직 반영되지 않은 상태이므로, BeginPlay에서 ApplySightConfig를 다시 호출한다.
	ApplySightConfig();

	// ── DamageConfig 구성 ─────────────────────────────────────────────────────
	// Damage 감각: ABWEnemy::TakeDamage에서 UAISense_Damage::ReportDamageEvent를 호출하면
	// 이 감각이 자극을 수신해 HandlePerceptionUpdated 콜백을 발생시킨다.
	// 타깃 결정 SSOT는 Sight 기반 Target 블랙보드 키를 유지(DominantSense = Sight).
	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	AIPerception->ConfigureSense(*DamageConfig);

	AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());
}

void ABWEnemyAIController::ApplySightConfig()
{
	if (!SightConfig || !AIPerception)
	{
		return;
	}

	SightConfig->SightRadius                   = SightRadius;
	SightConfig->LoseSightRadius               = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees  = VisionAngleDegrees;
	SightConfig->SetMaxAge(SightMaxAge);

	AIPerception->ConfigureSense(*SightConfig);
}

void ABWEnemyAIController::BeginPlay()
{
	Super::BeginPlay();

	// BP 클래스 디폴트 프로퍼티 오버라이드가 완전히 적용된 시점에 SightConfig를 재적용한다.
	// 생성자에서의 1차 적용은 엔진 기본값 기준이므로, 이 호출이 BP 튜닝값을 최종 반영한다.
	ApplySightConfig();
}

void ABWEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// BehaviorTree 실행 — null 가드 필수(에셋 미지정 시 크래시 방지)
	if (BehaviorTreeAsset)
	{
		RunBehaviorTree(BehaviorTreeAsset);
	}
	else
	{
		UE_LOG(LogBWAI, Warning,
			TEXT("[BWEnemyAIController] BehaviorTreeAsset이 설정되지 않았습니다. (%s)"),
			*GetName());
	}

	// Perception 이벤트 바인딩 — 즉각 반응을 위해 등록.
	// UFUNCTION 선언 없으면 AddDynamic이 조용히 실패하니 주의.
	if (AIPerception)
	{
		AIPerception->OnTargetPerceptionUpdated.AddDynamic(
			this, &ABWEnemyAIController::HandlePerceptionUpdated);
	}

	// OnPossess에서 CachedEnemy 캐시 — 매 UpdateTarget 호출마다 Cast를 피한다(CLAUDE.md 4.1).
	CachedEnemy = Cast<ABWEnemy>(InPawn);

	// 초기 이동 속도 = 순찰 속도. 이후 추격 진입 시 UpdateTarget에서 ChaseSpeed로 전환.
	bIsChasing = false;
	ApplyMovementSpeed(PatrolSpeed);

	// UpdateTarget 반복 타이머 시작 — 안전망(이벤트 누락 보완) + 초기 상태 동기화.
	// BT/Blackboard 초기화 타이밍과 겹칠 수 있으므로 UpdateTarget 내부에서 null 가드한다.
	GetWorldTimerManager().SetTimer(
		TargetTimerHandle,
		this,
		&ABWEnemyAIController::UpdateTarget,
		TargetUpdateInterval,
		/*bLoop=*/true);
}

void ABWEnemyAIController::ApplyMovementSpeed(float NewSpeed)
{
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
	{
		if (UCharacterMovementComponent* MoveComp = ControlledCharacter->GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed = NewSpeed;
		}
	}
}

void ABWEnemyAIController::OnUnPossess()
{
	// 타이머 정리 — 댕글링 콜백 방지 (CLAUDE.md 4.3)
	GetWorldTimerManager().ClearTimer(TargetTimerHandle);

	// Perception 델리게이트 해제
	if (AIPerception)
	{
		AIPerception->OnTargetPerceptionUpdated.RemoveDynamic(
			this, &ABWEnemyAIController::HandlePerceptionUpdated);
	}

	// BehaviorTree 정지
	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent))
	{
		BTComp->StopLogic(TEXT("Controller UnPossess"));
	}

	// CachedEnemy 무효화 — 댕글링 참조 방지.
	CachedEnemy.Reset();

	Super::OnUnPossess();
}

void ABWEnemyAIController::UpdateTarget()
{
	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB)
	{
		// BT 초기화 전 첫 타이머 틱에서 발생 가능 — null 가드로 조용히 넘긴다.
		return;
	}

	if (!AIPerception)
	{
		return;
	}

	// ── 1) 시야 후보 ─────────────────────────────────────────────────────────
	// "기억하고 있는" 인지 액터 목록을 가져온다.
	// GetCurrentlyPerceivedActors(지금 보이는 것만)와 달리, GetKnownPerceivedActors는
	// 한 번 인지한 뒤 SightMaxAge가 만료되기 전까지 액터를 유지한다.
	// → 시야가 잠깐 끊겨도 추격을 이어가고, MaxAge 동안 못 보면 잊고 순찰로 복귀한다.
	TArray<AActor*> PerceivedActors;
	AIPerception->GetKnownPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

	// 인지된 액터 중 첫 번째 ABWPlayerCharacter를 추격 대상으로 삼는다.
	// 단, 사망한 플레이어는 추격/공격 대상에서 제외한다.
	const APawn* SelfPawn = GetPawn();
	ABWPlayerCharacter* SightPlayer = nullptr;
	for (AActor* Actor : PerceivedActors)
	{
		// 자기 자신은 타깃 후보에서 원천 제외한다(자기 감지 → 제자리 공격 방지 방어 가드).
		if (Actor == SelfPawn)
		{
			continue;
		}

		if (ABWPlayerCharacter* Player = Cast<ABWPlayerCharacter>(Actor))
		{
			if (Player->IsDead())
			{
				continue;
			}

			SightPlayer = Player;
			break;
		}
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// ── 2) 시야로 보이면 "기억 타깃"을 갱신한다 ────────────────────────────────
	if (SightPlayer)
	{
		RememberedTarget = SightPlayer;
		ForgetTargetTime = Now + TargetMemoryDuration;
	}

	// ── 3) 유효 타깃 결정 — 시야 우선, 없으면 아직 안 잊은 기억 타깃 ──────────────
	// 기억 타깃은 시야 밖 피격(HandlePerceptionUpdated의 Damage 분기)으로도 설정된다.
	// 이 폴백이 없으면 피격으로 막 잡은 타깃이 다음 0.1초 폴링에서 즉시 지워진다(플리커 원인).
	AActor* EffectiveTarget = SightPlayer;
	if (!EffectiveTarget && RememberedTarget.IsValid() && Now < ForgetTargetTime)
	{
		if (ABWPlayerCharacter* Remembered = Cast<ABWPlayerCharacter>(RememberedTarget.Get()))
		{
			if (!Remembered->IsDead())
			{
				EffectiveTarget = Remembered;
			}
		}
	}

	// ── 4) 리쉬(leash) — 실제 거리가 LoseSightRadius를 넘으면 추격을 포기한다 ──────
	// GetKnownPerceivedActors는 시야가 끊겨도 MaxAge 동안 액터를 유지하므로, 거리 컷오프로
	// "너무 멀어지면 포기"를 결정적으로 보장한다(무한 추격 방지).
	if (EffectiveTarget)
	{
		if (const APawn* ControlledPawn = GetPawn())
		{
			const float DistSq = FVector::DistSquared(
				ControlledPawn->GetActorLocation(), EffectiveTarget->GetActorLocation());
			if (DistSq > FMath::Square(LoseSightRadius))
			{
				EffectiveTarget = nullptr;
				RememberedTarget.Reset();
			}
		}
	}

	// ── 5) 기억 만료/무효 정리 ─────────────────────────────────────────────────
	if (!EffectiveTarget)
	{
		RememberedTarget.Reset();
	}

	// ── 6) 블랙보드 반영 ───────────────────────────────────────────────────────
	const bool bHasTarget = (EffectiveTarget != nullptr);
	if (bHasTarget)
	{
		BB->SetValueAsObject(TargetBlackboardKey, EffectiveTarget);
	}
	else
	{
		BB->ClearValue(TargetBlackboardKey);
	}

	// 순찰↔추격 전환 시에만 이동 속도 변경 + HP 바 표시/숨김을 처리한다(매 타이머 틱 재설정 방지).
	if (bHasTarget != bIsChasing)
	{
		bIsChasing = bHasTarget;
		ApplyMovementSpeed(bIsChasing ? ChaseSpeed : PatrolSpeed);

		// CachedEnemy를 통해 HP 바 표시/숨김 전환. 추격 시작 → ShowHealthBar, 종료 → HideHealthBar.
		if (CachedEnemy.IsValid())
		{
			if (bIsChasing)
			{
				CachedEnemy->ShowHealthBar();
			}
			else
			{
				CachedEnemy->HideHealthBar();
			}
		}
	}
}

void ABWEnemyAIController::HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// 자극 종류를 확인해 Sight와 Damage를 분리 처리한다 (ue-reviewer [D]).
	// ── Sight 자극 ────────────────────────────────────────────────────────────
	// Sight known-list 기반 UpdateTarget이 Target 블랙보드 키 SSOT이므로
	// Sight 이벤트에서만 기존 UpdateTarget()을 호출해 순찰↔추격 전환 로직을 실행한다.
	const FAISenseID SightSenseID  = UAISense::GetSenseID<UAISense_Sight>();
	const FAISenseID DamageSenseID = UAISense::GetSenseID<UAISense_Damage>();

	if (Stimulus.Type == SightSenseID)
	{
		// 시야 이벤트: 기존 동작 그대로 — Known-list를 읽어 Target 키 갱신.
		UpdateTarget();
		return;
	}

	// ── Damage 자극 ───────────────────────────────────────────────────────────
	// 시야 밖에서 피격(Damage 자극)됐을 때, 데미지 유발자(플레이어)를 "기억 타깃"으로 잡고
	// UpdateTarget으로 일원화 반영한다. UpdateTarget이 시야 우선 + 기억 폴백 + 전환/HP바를
	// 모두 처리하므로(SSOT), 여기서 블랙보드를 직접 쓰지 않는다.
	// 기억 타깃을 먼저 세팅하므로, 시야가 비어 있어도 이번 UpdateTarget에서 추격이 시작되고
	// 다음 0.1초 폴링에서도 ForgetTargetTime 전까지 유지된다(즉시 지워지던 플리커 해결).
	if (Stimulus.Type == DamageSenseID)
	{
		ABWPlayerCharacter* Player = Cast<ABWPlayerCharacter>(Actor);
		if (!Player || Player->IsDead())
		{
			return;
		}

		RememberedTarget = Player;
		ForgetTargetTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) + TargetMemoryDuration;

		UpdateTarget();
	}
}

AActor* ABWEnemyAIController::GetCurrentTarget() const
{
	if (const UBlackboardComponent* BB = GetBlackboardComponent())
	{
		return Cast<AActor>(BB->GetValueAsObject(TargetBlackboardKey));
	}
	return nullptr;
}
