// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BWEnemyAIController.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/BWPlayerCharacter.h"

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

	// "기억하고 있는" 인지 액터 목록을 가져온다.
	// GetCurrentlyPerceivedActors(지금 보이는 것만)와 달리, GetKnownPerceivedActors는
	// 한 번 인지한 뒤 SightMaxAge가 만료되기 전까지 액터를 유지한다.
	// → 시야가 잠깐 끊겨도 추격을 이어가고, MaxAge 동안 못 보면 잊고 순찰로 복귀한다.
	TArray<AActor*> PerceivedActors;
	AIPerception->GetKnownPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

	// 인지된 액터 중 첫 번째 ABWPlayerCharacter를 추격 대상으로 삼는다.
	ABWPlayerCharacter* FoundPlayer = nullptr;
	for (AActor* Actor : PerceivedActors)
	{
		if (ABWPlayerCharacter* Player = Cast<ABWPlayerCharacter>(Actor))
		{
			FoundPlayer = Player;
			break;
		}
	}

	// 리쉬(leash) — 기억하고 있어도 실제 거리가 LoseSightRadius를 넘으면 추격을 포기한다.
	// GetKnownPerceivedActors는 시야가 끊겨도 MaxAge 동안 액터를 유지하므로, 추격 중 거리를
	// 좁혀 재인지되면 age가 리셋되어 MaxAge가 영원히 만료되지 않는 무한 추격이 발생한다.
	// 거리 기준 컷오프로 "너무 멀어지면 포기"를 결정적으로 보장한다.
	if (FoundPlayer)
	{
		if (const APawn* ControlledPawn = GetPawn())
		{
			const float DistSq = FVector::DistSquared(
				ControlledPawn->GetActorLocation(), FoundPlayer->GetActorLocation());
			if (DistSq > FMath::Square(LoseSightRadius))
			{
				FoundPlayer = nullptr;
			}
		}
	}

	const bool bHasTarget = (FoundPlayer != nullptr);
	if (bHasTarget)
	{
		BB->SetValueAsObject(TargetBlackboardKey, FoundPlayer);
	}
	else
	{
		BB->ClearValue(TargetBlackboardKey);
	}

	// 순찰↔추격 전환 시에만 이동 속도를 변경한다(매 타이머 틱 재설정 방지).
	if (bHasTarget != bIsChasing)
	{
		bIsChasing = bHasTarget;
		ApplyMovementSpeed(bIsChasing ? ChaseSpeed : PatrolSpeed);
	}
}

void ABWEnemyAIController::HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// 인지 이벤트 발생 즉시 UpdateTarget을 호출해 타이머 주기를 기다리지 않고 즉각 반응.
	UpdateTarget();
}
