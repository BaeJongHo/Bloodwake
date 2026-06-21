// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Animation/AnimMontage.h"
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
	 * 공격을 1회 수행한다. 완료 콜백(OnEnded)을 받아 호출자(BT 등)가 종료를 동기화한다.
	 * 구현체는 적합한 공격 몽타주를 재생하고, 종료 시 OnEnded를 호출(Montage_SetEndDelegate)해야 한다.
	 * 스태미나 부족·몽타주 재생 실패 시에도 반드시 OnEnded를 즉시 호출해 BT Latent 교착을 방지한다.
	 * OnEnded를 값으로 받는다 — Montage_SetEndDelegate가 비const 참조를 요구하므로.
	 */
	virtual void PerformAttack(FOnMontageEnded OnEnded) = 0;
};
