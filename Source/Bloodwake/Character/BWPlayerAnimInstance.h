// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BWPlayerAnimInstance.generated.h"

class ABWPlayerCharacter;
class UCharacterMovementComponent;

/**
 * ABWPlayerCharacter 전용 애님 인스턴스.
 * 캐릭터/이동 컴포넌트를 한 번 캐시해 두고, 워커 스레드 업데이트에서
 * 로코모션 블렌드용 값(지면 속도·이동 방향·낙하/이동 여부)을 갱신한다.
 * 락온 스트레이프·콤보·회피 등 전투 상태는 후속 작업에서 확장한다.
 */
UCLASS()
class BLOODWAKE_API UBWPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UBWPlayerAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

protected:
	/** 소유 플레이어 캐릭터. NativeInitializeAnimation에서 한 번 캐시한다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ABWPlayerCharacter> OwningCharacter;

	/** 소유 캐릭터의 이동 컴포넌트. 속도/낙하 상태 조회용으로 캐시한다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	/** 수평 이동 속도(cm/s). 이동 블렌드스페이스의 속도 축에 사용한다. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	float GroundSpeed = 0.0f;

	/** 액터 전방 기준 이동 방향(-180~180도). 락온 스트레이프 블렌드스페이스에 사용한다. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	float Direction = 0.0f;

	/** 이동 입력이 있고 속도가 충분해 이동 애니메이션을 재생해야 하는지 여부. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	bool bShouldMove = false;

	/** 이동 입력(가속)이 들어오고 있는지 여부. 관성으로만 미끄러지는 상태(false)와 구분해 정지/시작 전이에 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	bool bIsAccelerating = false;

	/** 걷기 구간 여부(이동 중 + 속도 ≤ WalkSpeedThreshold). 걷기/달리기 상태 전이·발소리 분기에 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	bool bIsWalking = false;

	/** 공중에 떠 있는지(낙하/점프) 여부. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	bool bIsFalling = false;

	/** 수직 속도(cm/s). 양수면 상승(점프 시작), 음수면 하강(낙하). 점프/낙하 상태 분기에 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	float VerticalSpeed = 0.0f;

	/** 공중 + 상승 중(점프해서 올라가는 구간) 여부. 점프 시작 모션 진입 조건으로 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	bool bIsJumping = false;

protected:
	/** bShouldMove 판정 기준 속도(cm/s). 이 값 이하의 미세 속도는 이동으로 보지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Config", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MovingSpeedThreshold = 3.0f;

	/** 걷기/달리기 구분 기준 속도(cm/s). 이 값 이하면 걷기로 본다. 무기·상태별 보행 속도에 맞춰 조정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Config", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float WalkSpeedThreshold = 165.0f;
};
