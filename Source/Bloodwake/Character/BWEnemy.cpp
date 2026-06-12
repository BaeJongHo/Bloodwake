// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/BWEnemy.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"
#include "Combat/BWAttributeComponent.h"
#include "Character/BWStateComponent.h"

ABWEnemy::ABWEnemy()
{
	// 적은 이벤트/입력 기반이므로 매 프레임 틱 비활성.
	PrimaryActorTick.bCanEverTick = false;

	// AttributeComponent — 체력·스태미나·기력 관리 (플레이어와 동일 패턴)
	AttributeComponent = CreateDefaultSubobject<UBWAttributeComponent>(TEXT("AttributeComponent"));

	// StateComponent — 행동 상태(GameplayTag) 관리
	StateComponent = CreateDefaultSubobject<UBWStateComponent>(TEXT("StateComponent"));
}

void ABWEnemy::BeginPlay()
{
	Super::BeginPlay();

	// 사망 델리게이트 구독. 체력 0 도달 시 HandleDeath 호출.
	if (AttributeComponent)
	{
		AttributeComponent->OnDeath.AddDynamic(this, &ABWEnemy::HandleDeath);
	}
}

void ABWEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 댕글링 콜백 방지 — 델리게이트 해제 (규약 4.3)
	if (AttributeComponent)
	{
		AttributeComponent->OnDeath.RemoveDynamic(this, &ABWEnemy::HandleDeath);
	}

	Super::EndPlay(EndPlayReason);
}

// ── TakeDamage 오버라이드 ─────────────────────────────────────────────────────

float ABWEnemy::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// 이미 사망한 적에게 추가 타격을 무시한다.
	if (bIsDead)
	{
		return 0.f;
	}

	// 엔진 표준 처리를 통해 실제 데미지 수치를 획득한다.
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// FPointDamageEvent에서 ImpactPoint/ShotDirection 추출
	FVector ImpactPoint = GetActorLocation();
	FVector ShotDirection = GetActorForwardVector(); // 폴백: 정면에서 맞은 것으로 처리

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		ImpactPoint = PointEvent.HitInfo.ImpactPoint;

		if (!PointEvent.ShotDirection.IsNearlyZero())
		{
			ShotDirection = PointEvent.ShotDirection;
		}
	}

	// 데미지를 AttributeComponent에 위임
	if (AttributeComponent)
	{
		AttributeComponent->ApplyDamage(ActualDamage);
	}

	// 피격 이펙트·리액션 재생 (사망은 OnDeath 델리게이트가 별도 트리거)
	PlayHitEffects(ImpactPoint);
	PlayHitReaction(ShotDirection);

	return ActualDamage;
}

// ── HandleDeath ───────────────────────────────────────────────────────────────

void ABWEnemy::HandleDeath()
{
	// bIsDead 가드 — HandleDeath 재진입 방지
	if (bIsDead)
	{
		return;
	}

	EnableRagdoll();
}

// ── private 구현부 ────────────────────────────────────────────────────────────

EBWHitDirection ABWEnemy::ComputeHitDirection(const FVector& ShotDirection) const
{
	// ShotDirection이 거의 0이면 기본값(Front) 반환
	if (ShotDirection.IsNearlyZero())
	{
		return EBWHitDirection::Front;
	}

	// 피격자 → 공격자 방향 (수평 평면 투영)
	FVector FromAttacker = -ShotDirection;
	FromAttacker.Z = 0.f;
	if (!FromAttacker.Normalize())
	{
		return EBWHitDirection::Front;
	}

	FVector Fwd = GetActorForwardVector();
	Fwd.Z = 0.f;
	Fwd.Normalize();

	FVector Right = GetActorRightVector();
	Right.Z = 0.f;
	Right.Normalize();

	const float ForwardDot = FVector::DotProduct(Fwd, FromAttacker);
	const float RightDot   = FVector::DotProduct(Right, FromAttacker);

	// COS45 ≈ 0.707 — 앞뒤를 ±45° 콘으로, 나머지를 좌우로 분류
	constexpr float COS45 = 0.707f;

	if (ForwardDot >= COS45)
	{
		return EBWHitDirection::Front;
	}
	else if (ForwardDot <= -COS45)
	{
		return EBWHitDirection::Back;
	}
	else if (RightDot > 0.f)
	{
		return EBWHitDirection::Right;
	}
	else
	{
		return EBWHitDirection::Left;
	}
}

void ABWEnemy::PlayHitReaction(const FVector& ShotDirection)
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	const EBWHitDirection Direction = ComputeHitDirection(ShotDirection);

	UAnimMontage* MontageToPlay = nullptr;
	switch (Direction)
	{
	case EBWHitDirection::Front: MontageToPlay = HitReactFrontMontage; break;
	case EBWHitDirection::Back:  MontageToPlay = HitReactBackMontage;  break;
	case EBWHitDirection::Left:  MontageToPlay = HitReactLeftMontage;  break;
	case EBWHitDirection::Right: MontageToPlay = HitReactRightMontage; break;
	}

	// 해당 방향 몽타주가 없으면 Front로 폴백
	if (!MontageToPlay)
	{
		MontageToPlay = HitReactFrontMontage;
	}

	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWEnemy] PlayHitReaction: 히트 리액션 몽타주가 설정되지 않았습니다. (%s)"),
			*GetName());
		return;
	}

	AnimInstance->Montage_Play(MontageToPlay);
}

void ABWEnemy::PlayHitEffects(const FVector& ImpactPoint)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Cascade 파티클 VFX 스폰
	if (HitVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			World,
			HitVFX,
			ImpactPoint,
			FRotator::ZeroRotator,
			/*bAutoDestroy=*/true
		);
	}

	// 사운드 재생
	if (HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, HitSound, ImpactPoint);
	}
}

void ABWEnemy::EnableRagdoll()
{
	// 1) 사망 플래그 설정 (이후 TakeDamage 재진입 차단)
	bIsDead = true;

	// 2) 진행 중인 행동 상태 태그 모두 해제
	if (StateComponent)
	{
		StateComponent->ClearAllStateTags();
	}

	// 3) 진행 중인 몽타주 정지 (히트 리액션이 랙돌과 충돌하지 않도록)
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		AnimInstance->Montage_Stop(0.1f);
	}

	// 4) 이동 컴포넌트 비활성화
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->DisableMovement();
		MoveComp->StopMovementImmediately();
	}

	// 5) 캡슐 콜리전 OFF (랙돌 중 캡슐 충돌 제거)
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 6) 메시 물리 ON — 랙돌 전환
	//    콜리전 프로파일 설정 → 충돌 활성화 → 물리 시뮬레이션 시작 순서 준수
	if (USkeletalMeshComponent* EnemyMesh = GetMesh())
	{
		EnemyMesh->SetCollisionProfileName(RagdollCollisionProfile);
		EnemyMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		EnemyMesh->SetSimulatePhysics(true);
	}

	// 7) 시체 자동 제거 (CorpseLifeSpan > 0 인 경우만)
	if (CorpseLifeSpan > 0.f)
	{
		SetLifeSpan(CorpseLifeSpan);
	}
}
