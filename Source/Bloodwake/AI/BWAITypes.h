// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BWAITypes.generated.h"

/**
 * 적 AI의 현재 행동 상태 열거형.
 * UBWBTService_SelectBehavior가 매 주기 평가해 Blackboard Behavior 키(Enum)에 기록한다.
 * BB_Enemy의 Enum 키 타입으로 이 UEnum을 지정한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UENUM(BlueprintType)
enum class EBWAIBehavior : uint8
{
	Idle        UMETA(DisplayName = "Idle"),         // 타깃 미감지 & 순찰점 없음 — 제자리 대기
	Patrol      UMETA(DisplayName = "Patrol"),       // 타깃 미감지 & 순찰점 있음 — 순찰
	Approach    UMETA(DisplayName = "Approach"),     // 타깃 감지 & 공격 사거리 밖 — 추격
	MeleeAttack UMETA(DisplayName = "MeleeAttack"),  // 타깃 감지 & 공격 사거리 안 — 근접 공격
	Stunned     UMETA(DisplayName = "Stunned"),      // 패링 당함/스턴 — 행동 불능, 제자리 정지
	Strafe      UMETA(DisplayName = "Strafe"),       // 스태미나 고갈 — 타깃 주시하며 간보기 이동
};
