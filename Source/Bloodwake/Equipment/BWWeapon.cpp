// Fill out your copyright notice in the Description page of Project Settings.

#include "Equipment/BWWeapon.h"

ABWWeapon::ABWWeapon()
{
	// 무기 전용 초기화. 공통 설정은 ABWEquipItem 생성자가 처리한다.
	PrimaryActorTick.bCanEverTick = false;
}

FName ABWWeapon::GetHandSocketOverride() const
{
	if (CombatType == EBWCombatType::TwoHanded)
	{
		return TwoHandedEquipSocketName;
	}
	return NAME_None;
}

FName ABWWeapon::GetHolsterSocketOverride() const
{
	if (CombatType == EBWCombatType::TwoHanded)
	{
		return TwoHandedUnequipSocketName;
	}
	return NAME_None;
}
