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
