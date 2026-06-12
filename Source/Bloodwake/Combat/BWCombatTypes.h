// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BWCombatTypes.generated.h"

/**
 * 장비 부착 목적지(손=장착 자세, 등=보관).
 * UBWAnimNotify_AttachEquip 노티파이 파라미터 및 UBWCombatComponent::AttachSlotToSocket API에서 사용된다.
 * CombatComponent / AnimNotify 양측이 공통으로 include 하는 경량 타입 헤더.
 */
UENUM(BlueprintType)
enum class EBWAttachDestination : uint8
{
	Hand UMETA(DisplayName = "Hand"),
	Back UMETA(DisplayName = "Back"),
};

/**
 * 피격 방향 4분면.
 * ShotDirection(공격자→피격자)을 피격자 forward/right 내적으로 분류한다.
 * ABWEnemy::ComputeHitDirection 에서 사용한다.
 */
UENUM(BlueprintType)
enum class EBWHitDirection : uint8
{
	Front UMETA(DisplayName = "Front"),
	Back  UMETA(DisplayName = "Back"),
	Left  UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right"),
};
