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
 * 캐릭터의 현재 전투 형태.
 * 무기(ABWWeapon)가 EditDefaultsOnly로 자신의 타입을 보유하고,
 * UBWCombatComponent가 현재 활성 타입을 SSOT로 캐시·노출한다.
 * 빈손이면 MeleeFists로 자동 진입한다(별도 토글 없음).
 */
UENUM(BlueprintType)
enum class EBWCombatType : uint8
{
	SwordAndShield UMETA(DisplayName = "Sword And Shield"),
	TwoHanded      UMETA(DisplayName = "Two Handed"),
	MeleeFists     UMETA(DisplayName = "Melee Fists"),
};

/**
 * 히트 콜리전 슬롯 선택자.
 * UBWAnimNotifyState_WeaponCollision 디테일 트랙에서 어느 손/무기 콜리전을 켤지 지정한다.
 * 무기 전투: Main = 주무기. 맨손: Main = 오른손, Second = 왼손(매핑은 CombatComponent가 결정).
 */
UENUM(BlueprintType)
enum class EBWWeaponSlotSelector : uint8
{
	Main   UMETA(DisplayName = "Main"),
	Second UMETA(DisplayName = "Second"),
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
