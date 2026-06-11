// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/BWEquipItem.h"
#include "BWShield.generated.h"

/**
 * 방패 액터의 추상 베이스 클래스. ABWEquipItem에서 파생한다.
 * 방패 고유 데이터(가드 데미지 감쇄율·가드 스태미나 소모)를 BP 자식 설정 슬롯으로 노출한다.
 * 패링/가드 판정 로직은 후속 작업에서 구현한다.
 */
UCLASS(Abstract, Blueprintable)
class BLOODWAKE_API ABWShield : public ABWEquipItem
{
	GENERATED_BODY()

public:
	ABWShield();

	/** 방패 슬롯을 반환한다. CombatComponent의 슬롯 라우팅에 사용된다. */
	virtual EBWEquipSlot GetEquipSlot() const override { return EBWEquipSlot::Shield; }

protected:
	/** 가드 시 데미지 감쇄 비율(0=무감쇄, 1=완전 차단). BP 자식에서 밸런싱. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield|Guard", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlockDamageReduction = 0.5f;

	/** 가드 1회 스태미나 소모량. BP 자식에서 밸런싱. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield|Guard", meta = (ClampMin = "0.0"))
	float GuardStaminaCost = 0.f;
};
