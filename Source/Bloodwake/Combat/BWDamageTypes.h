// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "BWDamageTypes.generated.h"

/**
 * 넉다운(쓰러짐) 데미지 타입 — 의미 태깅 전용. 멤버 없음.
 * 이 데미지 타입으로 맞은 ABWPlayerCharacter는 TakeDamage에서
 * 일반 4방향 히트 리액션 대신 넉다운(쓰러짐) 몽타주를 재생하고,
 * 그 동안 모든 조작이 차단된다.
 * UBWAnimNotify_BWKnockdown이 FPointDamageEvent 생성 시 이 타입을 지정한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS()
class BLOODWAKE_API UBWDamageType_Knockdown : public UDamageType
{
	GENERATED_BODY()
};
