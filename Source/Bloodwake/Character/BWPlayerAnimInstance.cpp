// Fill out your copyright notice in the Description page of Project Settings.


#include "BWPlayerAnimInstance.h"

#include "BWPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

UBWPlayerAnimInstance::UBWPlayerAnimInstance()
{
}

void UBWPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 소유 폰을 한 번만 캐스팅해 캐시한다(매 업데이트 Cast 금지 — 4.1 규약).
	OwningCharacter = Cast<ABWPlayerCharacter>(TryGetPawnOwner());
	if (OwningCharacter)
	{
		MovementComponent = OwningCharacter->GetCharacterMovement();
	}
}

void UBWPlayerAnimInstance::AnimNotify_RollEnd()
{
	// 캐시한 소유 캐릭터에 회피 종료를 위임한다(상태 태그 관리는 캐릭터/StateComponent 책임).
	if (OwningCharacter)
	{
		OwningCharacter->EndRoll();
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

	// 가속 입력 존재 여부를 한 번만 조회해 재사용한다.
	bIsAccelerating = !MovementComponent->GetCurrentAcceleration().IsNearlyZero();

	// 가속 입력이 있고 실제로 움직이고 있을 때만 이동 애니메이션을 재생한다.
	bShouldMove = GroundSpeed > MovingSpeedThreshold && bIsAccelerating;

	// 이동 중이면서 임계 속도 이하면 걷기로 분류한다.
	bIsWalking = bShouldMove && GroundSpeed <= WalkSpeedThreshold;

	// 액터 전방 기준 이동 방향(도). 스트레이프 블렌드스페이스용으로 직접 계산해
	// AnimGraphRuntime(UKismetAnimationLibrary) 의존성을 추가하지 않는다.
	if (!Velocity.IsNearlyZero())
	{
		const FMatrix RotationMatrix = FRotationMatrix(OwningCharacter->GetActorRotation());
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
