// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Animation/AnimMontage.h"
#include "Combat/BWAttackTypes.h"
#include "BWCombatInterface.generated.h"

/**
 * "공격을 수행할 수 있는 객체"의 공통 계약.
 * ABWPlayerCharacter / ABWEnemy 양쪽이 구현한다.
 * BT(UBWBTTask_PerformAttack)가 인터페이스 경유로 공격을 요청하고, FOnMontageEnded 콜백으로 완료를 동기화한다.
 * 기존 IBWTargetingInterface(BWTargetingInterface.h)와 동일한 UE 표준 인터페이스 패턴.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UBWCombatInterface : public UInterface
{
	GENERATED_BODY()
};

class BLOODWAKE_API IBWCombatInterface
{
	GENERATED_BODY()

public:
	/**
	 * 지정한 공격 종류(AttackType)로 공격을 1회 수행한다. 완료 콜백(OnEnded)을 받아 호출자(BT 등)가 종료를 동기화한다.
	 * 구현체는 AttackType에 해당하는 공격 몽타주를 재생하고, 종료 시 OnEnded를 호출(Montage_SetEndDelegate)해야 한다.
	 * AttackType은 BT(UBWBTTask_PerformAttack)가 노드별로 지정한다(Light/Heavy/Special 등).
	 * 스태미나 부족·몽타주 재생 실패 시에도 반드시 OnEnded를 즉시 호출해 BT Latent 교착을 방지한다.
	 * OnEnded를 값으로 받는다 — Montage_SetEndDelegate가 비const 참조를 요구하므로.
	 */
	virtual void PerformAttack(EBWAttackType AttackType, FOnMontageEnded OnEnded) = 0;

	/**
	 * 현재 무적(i-frame) 상태인지 반환한다.
	 * TakeDamage 최상단에서 조기 반환 판정에 사용한다.
	 * 기본 구현은 false — 무적을 지원하지 않는 구현체는 오버라이드하지 않아도 된다.
	 * 순수 가상으로 두지 않는다 — ABWEnemy 계열 전부가 즉시 컴파일 에러가 되기 때문.
	 */
	virtual bool IsInvincible() const { return false; }

	/**
	 * 무적 상태를 설정한다. UBWAnimNotifyState_Invincibility가 몽타주 윈도우 진입/이탈 시 호출한다.
	 * 기본 구현은 무동작 — 무적을 지원하는 구현체(ABWPlayerCharacter)만 오버라이드한다.
	 * 향후 보스 회피 i-frame이 필요할 때 ABWEnemy에 동일 오버라이드를 추가하는 것으로 확장된다.
	 */
	virtual void SetInvincible(bool bInInvincible) {}
};
