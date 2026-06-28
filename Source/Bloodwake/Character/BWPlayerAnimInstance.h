// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Combat/BWCombatTypes.h"
#include "BWPlayerAnimInstance.generated.h"

class ACharacter;
class ABWPlayerCharacter;
class UCharacterMovementComponent;
class UBWCombatComponent;

/**
 * 플레이어·적 공용 애님 인스턴스.
 * 클래스명은 이전 호환성을 위해 "Player"를 유지하되, NativeInitializeAnimation에서
 * ACharacter 공통 API로 캐싱해 Enemy에서도 로코모션이 정상 동작한다(결정사항 7).
 * 플레이어 전용 기능(EndRoll)은 CachedPlayerCharacter 약참조를 통해 처리한다.
 * 캐릭터/이동 컴포넌트를 한 번 캐시해 두고, 워커 스레드 업데이트에서
 * 로코모션 블렌드용 값(지면 속도·이동 방향·낙하/이동 여부)을 갱신한다.
 */
UCLASS()
class BLOODWAKE_API UBWPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UBWPlayerAnimInstance();

	virtual void NativeInitializeAnimation() override;
	/**
	 * 게임 스레드에서 호출된다. CombatComponent의 bIsWeaponDrawn/CurrentCombatType을
	 * 멤버(CachedWeaponDrawn/CachedCombatType)에 미리 캐시한다.
	 * NativeThreadSafeUpdateAnimation은 이 캐시값만 읽어 워커 스레드 안전을 보장한다.
	 */
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	/**
	 * 회피(Roll) 몽타주에 배치한 커스텀(이름 기반) AnimNotify "RollEnd"가 호출하는 함수.
	 * 엔진은 이름 기반 노티파이를 소유 AnimInstance의 AnimNotify_<NotifyName> 함수로 디스패치한다.
	 * 플레이어 캐릭터(CachedPlayerCharacter)에 한해 EndRoll을 호출한다.
	 * Enemy에서 사용하면 null이므로 무동작(안전).
	 */
	UFUNCTION()
	void AnimNotify_RollEnd();

	/**
	 * 블로킹 히트(가드 피격 경직) 몽타주에 배치한 커스텀(이름 기반) AnimNotify "BlockingHitEnd"가 호출하는 함수.
	 * 경직 회복 프레임에서 플레이어 캐릭터의 EndBlockingHit를 호출해 BlockingHit 태그 해제 및 입력 잠금 복원을 수행한다.
	 * Enemy에서는 CachedPlayerCharacter가 null이므로 무동작(안전).
	 */
	UFUNCTION()
	void AnimNotify_BlockingHitEnd();

	/**
	 * 패링 몽타주에 배치한 커스텀(이름 기반) AnimNotify "ParryEnd"가 호출하는 함수.
	 * 원하는 회복 프레임에서 플레이어 캐릭터의 EndParry를 호출해 Parrying 태그 해제 및 이동 입력 잠금 복원을 수행한다.
	 * 몽타주 끝까지 기다리지 않고 입력을 돌려주기 위한 진입점이다.
	 * Enemy에서는 CachedPlayerCharacter가 null이므로 무동작(안전).
	 */
	UFUNCTION()
	void AnimNotify_ParryEnd();

protected:
	/**
	 * 소유 ACharacter. NativeInitializeAnimation에서 한 번 캐시한다.
	 * 플레이어/적 양쪽에서 동작하도록 ACharacter로 일반화(결정사항 7).
	 * 로코모션 방향 계산(GetActorRotation)에 사용한다.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ACharacter> OwningCharacter;

	/** 소유 캐릭터의 이동 컴포넌트. 속도/낙하 상태 조회용으로 캐시한다. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	/** 수평 이동 속도(cm/s). 이동 블렌드스페이스의 속도 축에 사용한다. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	float GroundSpeed = 0.0f;

	/** 액터 전방 기준 이동 방향(-180~180도). 락온 스트레이프 블렌드스페이스에 사용한다. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	float Direction = 0.0f;

	/** 이동 속도가 MovingSpeedThreshold를 초과할 때 이동 애니메이션을 재생해야 하는지 여부. 속도 단독 판정(AI 폰 호환). */
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

	/**
	 * 현재 락온 중인지 여부. ABP에서 스트레이프 블렌드스페이스 전환 조건으로 사용한다.
	 * NativeThreadSafeUpdateAnimation에서 CachedPlayerCharacter->IsLockedOn()(캐시 bool)을 pull한다.
	 * Enemy에서는 항상 false(락온 중이 아님)로 유지한다.
	 * Thread-safe: 캐시된 bool을 읽어 워커 스레드 안전.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion", meta = (AllowPrivateAccess = "true"))
	bool bIsLockedOn = false;

	/**
	 * 손에 무기를 들고 있는지. ABP 무기/맨손 로코모션 분기.
	 * NativeThreadSafeUpdateAnimation에서 CachedCombatComponent->IsWeaponDrawn()을 pull한다.
	 * Thread-safe: CombatComponent가 갱신하는 POD bool을 읽는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	bool bIsWeaponDrawn = false;

	/**
	 * 현재 전투 형태. ABP 무기 종류별 포즈/블렌드 분기.
	 * NativeThreadSafeUpdateAnimation에서 CachedCombatComponent->GetCurrentCombatType()을 pull한다.
	 * Thread-safe: CombatComponent가 갱신하는 POD enum을 읽는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	EBWCombatType CurrentCombatType = EBWCombatType::MeleeFists;

	/**
	 * 블로킹(방패 가드) 중인지 여부. ABP 블로킹 포즈/레이어 전환 조건으로 사용한다.
	 * NativeThreadSafeUpdateAnimation에서 bCachedShouldBlocking을 복사한다.
	 * Thread-safe: 게임 스레드 캐시값만 읽는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	bool bShouldBlocking = false;

	/** bShouldMove 판정 기준 속도(cm/s). 이 값 이하의 미세 속도는 이동으로 보지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Config", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MovingSpeedThreshold = 3.0f;

	/** 걷기/달리기 구분 기준 속도(cm/s). 이 값 이하면 걷기로 본다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Config", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float WalkSpeedThreshold = 165.0f;

private:
	/**
	 * 소유 캐릭터의 CombatComponent 캐시. NativeInitializeAnimation에서 1회 취득.
	 * 약참조이므로 사용 전 IsValid 확인 필수.
	 */
	TWeakObjectPtr<UBWCombatComponent> CachedCombatComponent;

	/**
	 * 플레이어 캐릭터 전용 캐시. EndRoll 등 플레이어 전용 기능 호출에 사용.
	 * Enemy에서는 null이므로 IsValid 확인 필수.
	 * 약참조이므로 사용 전 IsValid 확인 필수.
	 */
	TWeakObjectPtr<ABWPlayerCharacter> CachedPlayerCharacter;

	/**
	 * 게임 스레드(NativeUpdateAnimation)에서 CombatComponent 값을 미리 읽어 두는 캐시.
	 * NativeThreadSafeUpdateAnimation(워커 스레드)은 이 값만 복사해 스레드 안전을 보장한다.
	 */
	bool bCachedWeaponDrawn = false;
	EBWCombatType CachedCombatType = EBWCombatType::MeleeFists;

	/** 플레이어 IsLockedOn 캐시. NativeUpdateAnimation(게임 스레드)에서 읽어 두는 캐시. */
	bool bCachedIsLockedOn = false;

	/** 블로킹 상태 캐시. NativeUpdateAnimation(게임 스레드)에서 IsBlocking() 값을 미리 읽어 둔다. */
	bool bCachedShouldBlocking = false;

	/**
	 * 액터 회전 캐시. NativeUpdateAnimation(게임 스레드)에서 OwningCharacter->GetActorRotation()을 읽어 저장.
	 * NativeThreadSafeUpdateAnimation(워커 스레드)은 이 POD 캐시만 사용해 스레드 안전을 보장한다.
	 */
	FRotator CachedActorRotation = FRotator::ZeroRotator;
};
