// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/BWEquipItem.h"
#include "BWWeapon.generated.h"

/**
 * 무기 액터의 추상 베이스 클래스. ABWEquipItem에서 파생한다.
 * 무기 고유 데이터(데미지·포이즈 데미지·스태미나 소모)를 BP 자식 설정 슬롯으로 노출한다.
 * 히트 판정 로직은 후속 작업(Combat/ AnimNotify 기반)에서 구현한다.
 */
UCLASS(Abstract, Blueprintable)
class BLOODWAKE_API ABWWeapon : public ABWEquipItem
{
	GENERATED_BODY()

public:
	ABWWeapon();

	/** 무기 슬롯을 반환한다. CombatComponent의 슬롯 라우팅에 사용된다. */
	virtual EBWEquipSlot GetEquipSlot() const override { return EBWEquipSlot::Weapon; }

protected:
	/** 기본 데미지. BP 자식에서 밸런싱. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats", meta = (ClampMin = "0.0"))
	float BaseDamage = 10.f;

	/** 포이즈 데미지. BP 자식에서 밸런싱. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats", meta = (ClampMin = "0.0"))
	float PoiseDamage = 0.f;

	/** 공격 1회 스태미나 소모량. BP 자식에서 밸런싱. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats", meta = (ClampMin = "0.0"))
	float StaminaCostPerSwing = 0.f;
};
