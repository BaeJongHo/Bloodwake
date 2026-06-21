// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/BWPlayerAnimInstance.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/BWPlayerCharacter.h"
#include "Combat/BWCombatComponent.h"

UBWPlayerAnimInstance::UBWPlayerAnimInstance()
{
}

void UBWPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 소유 폰을 ACharacter로 캐스팅한다(결정사항 7: 플레이어/적 공통 동작).
	// 이전 ABWPlayerCharacter 직접 캐스팅에서 변경 — Enemy에서도 로코모션이 동작하도록.
	OwningCharacter = Cast<ACharacter>(TryGetPawnOwner());
	if (OwningCharacter)
	{
		MovementComponent = OwningCharacter->GetCharacterMovement();

		// CombatComponent를 1회 캐시한다. bIsWeaponDrawn/CurrentCombatType pull에 사용.
		CachedCombatComponent = OwningCharacter->FindComponentByClass<UBWCombatComponent>();

		// 플레이어 캐릭터 전용 캐시 — null이면 Enemy이므로 무시.
		// EndRoll 등 플레이어 전용 기능 호출에 사용한다.
		CachedPlayerCharacter = Cast<ABWPlayerCharacter>(OwningCharacter);
	}
}

void UBWPlayerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 게임 스레드에서만 CombatComponent에 접근해 캐시를 갱신한다.
	// NativeThreadSafeUpdateAnimation(워커 스레드)은 이 캐시값을 읽어 스레드 안전을 보장한다.
	if (CachedCombatComponent.IsValid())
	{
		bCachedWeaponDrawn = CachedCombatComponent->IsWeaponDrawn();
		CachedCombatType   = CachedCombatComponent->GetCurrentCombatType();
	}

	// 플레이어 전용: 락온 상태 캐시 갱신.
	// Enemy에서는 CachedPlayerCharacter가 null이므로 bCachedIsLockedOn이 false로 유지된다.
	if (CachedPlayerCharacter.IsValid())
	{
		bCachedIsLockedOn = CachedPlayerCharacter->IsLockedOn();
	}
	else
	{
		bCachedIsLockedOn = false;
	}

	// 액터 회전 캐시 갱신. 워커 스레드(NativeThreadSafeUpdateAnimation)에서
	// OwningCharacter를 직접 역참조하지 않도록 게임 스레드에서 미리 읽어 둔다.
	if (OwningCharacter)
	{
		CachedActorRotation = OwningCharacter->GetActorRotation();
	}
}

void UBWPlayerAnimInstance::AnimNotify_RollEnd()
{
	// 플레이어 캐릭터에 한해 EndRoll을 호출한다.
	// Enemy에서는 CachedPlayerCharacter가 null이므로 무동작(안전).
	if (CachedPlayerCharacter.IsValid())
	{
		CachedPlayerCharacter->EndRoll();
	}
}

void UBWPlayerAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	// 에디터 프리뷰 등 소유 캐릭터가 없는 경우 안전하게 빠져나간다.
	if (!OwningCharacter || !MovementComponent)
	{
		return;
	}

	const FVector Velocity = MovementComponent->Velocity;

	// 수평 속도만 사용한다(낙하 중 수직 속도는 로코모션 블렌드에서 제외).
	GroundSpeed = Velocity.Size2D();

	bIsFalling = MovementComponent->IsFalling();

	// 수직 속도로 상승(점프)/하강(낙하)을 구분한다. IsFalling()만으로는 둘을 구분할 수 없다.
	VerticalSpeed = Velocity.Z;

	// 공중에 떠 있으면서 위로 올라가는 동안만 '점프 중'으로 본다(정점 통과 후엔 낙하로 전환).
	bIsJumping = bIsFalling && VerticalSpeed > 0.0f;

	// 가속 입력 존재 여부를 한 번만 조회해 재사용한다(bShouldMove 게이트에서는 미사용 — 결정사항 6).
	bIsAccelerating = !MovementComponent->GetCurrentAcceleration().IsNearlyZero();

	// bShouldMove: 속도 단독 판정(결정사항 6).
	// AI 폰은 PathFollowing으로 이동해 GetCurrentAcceleration()이 0이 될 수 있으므로
	// 속도(GroundSpeed)만으로 판정해 플레이어/적 공통 동작을 보장한다.
	bShouldMove = GroundSpeed > MovingSpeedThreshold;

	// 이동 중이면서 임계 속도 이하면 걷기로 분류한다.
	bIsWalking = bShouldMove && GroundSpeed <= WalkSpeedThreshold;

	// 락온 상태 pull. NativeUpdateAnimation(게임 스레드)에서 미리 캐시해 둔 값을 복사한다.
	// 워커 스레드에서 OwningCharacter에 직접 접근하지 않아 스레드 안전.
	bIsLockedOn = bCachedIsLockedOn;

	// 전투 상태 pull. NativeUpdateAnimation(게임 스레드)에서 미리 캐시해 둔 값을 복사한다.
	bIsWeaponDrawn    = bCachedWeaponDrawn;
	CurrentCombatType = CachedCombatType;

	// 액터 전방 기준 이동 방향(도). 스트레이프 블렌드스페이스용으로 직접 계산해
	// AnimGraphRuntime(UKismetAnimationLibrary) 의존성을 추가하지 않는다.
	// CachedActorRotation: 게임 스레드(NativeUpdateAnimation)에서 미리 캐시한 값을 사용해
	// 워커 스레드에서 OwningCharacter를 역참조하지 않도록 한다(스레드 안전).
	if (!Velocity.IsNearlyZero())
	{
		const FMatrix RotationMatrix = FRotationMatrix(CachedActorRotation);
		const FVector ForwardVector = RotationMatrix.GetScaledAxis(EAxis::X);
		const FVector RightVector = RotationMatrix.GetScaledAxis(EAxis::Y);
		const FVector NormalizedVelocity = Velocity.GetSafeNormal2D();

		const float ForwardCosAngle = FVector::DotProduct(ForwardVector, NormalizedVelocity);
		float ForwardDeltaDegrees = FMath::RadiansToDegrees(FMath::Acos(ForwardCosAngle));

		// 우측 성분 부호로 좌(-)/우(+)를 구분한다.
		if (FVector::DotProduct(RightVector, NormalizedVelocity) < 0.0f)
		{
			ForwardDeltaDegrees *= -1.0f;
		}

		Direction = ForwardDeltaDegrees;
	}
	else
	{
		Direction = 0.0f;
	}
}
