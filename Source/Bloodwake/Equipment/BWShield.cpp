// Fill out your copyright notice in the Description page of Project Settings.

#include "Equipment/BWShield.h"

ABWShield::ABWShield()
{
	// 방패 전용 초기화. 공통 설정은 ABWEquipItem 생성자가 처리한다.
	PrimaryActorTick.bCanEverTick = false;
}
