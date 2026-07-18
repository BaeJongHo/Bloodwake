// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

// 캐릭터 행동 상태/액션 네이티브 GameplayTag 선언.
// 정의(UE_DEFINE_GAMEPLAY_TAG_COMMENT)는 BWGameplayDefine.cpp에 있다.
// 사용: BWGameplayTags::Character_State_Sprint.GetTag()
namespace BWGameplayTags
{
	/** 기본(Normal) 상태 태그. 행동 상태(Sprint/Roll/Attack 등)가 없을 때의 기본값. 문자열: "Character.State.Normal" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_State_Normal);

	/** 질주(Sprint) 상태 태그. 문자열: "Character.State.Sprint" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_State_Sprint);

	/** 구르기(Roll) 상태 태그. 문자열: "Character.State.Roll" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_State_Roll);

	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_State_Death);

	/** 피격(Hit) 상태 태그. 히트 리액션 몽타주 재생 중을 나타낸다. 문자열: "Character.State.Hit" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_State_Hit);

	/** 장착(Equip) 액션 태그. 무기/방패 뽑기 모션이 진행 중임을 나타낸다. 문자열: "Character.Action.Equip" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Action_Equip);

	/** 해제(Unequip) 액션 태그. 무기/방패 넣기 모션이 진행 중임을 나타낸다. 문자열: "Character.Action.Unequip" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Action_Unequip);

	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Attack_Light);
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Attack_Running);
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Attack_Special);
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Attack_Heavy);

	/** 블로킹(Block) 상태 태그. 방패 가드 모션이 활성 상태임을 나타낸다. 문자열: "Character.State.Blocking" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_State_Blocking);

	/** 블로킹 히트 리액션(BlockingHit) 액션 태그. 가드 성공 후 피격 리액션 몽타주 진행 중. 문자열: "Character.Action.BlockingHit" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Action_BlockingHit);

	/** 패링(Parrying) 상태 태그. 플레이어가 패링 유효 윈도우 구간에 있음을 나타낸다. 문자열: "Character.State.Parrying" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_State_Parrying);

	/** 패링 당함(Parried) 상태 태그. 적이 플레이어 패링에 당해 경직 상태임을 나타낸다. 문자열: "Character.State.Parried" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_State_Parried);

	/** 패링 당함 리액션(ParriedHit) 액션 태그. 적의 패링 당함 리액션 몽타주 진행 중. 문자열: "Character.Action.ParriedHit" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Action_ParriedHit);

	/** 스턴(Stunned) 상태 태그. 적이 피격 후 확률적 스턴 상태에 빠진 것을 나타낸다. 문자열: "Character.State.Stunned" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_State_Stunned);

	/** 포션 마시기(DrinkingPotion) 상태 태그. 포션 마시기 몽타주 진행 중 — 공격/방어/점프/구르기/패링 금지. 문자열: "Character.State.DrinkingPotion" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_State_DrinkingPotion);

	/** 넉다운 피격(KnockdownHit) 상태 태그. 쓰러짐 몽타주 진행 중, 모든 입력 차단. 문자열: "Character.State.KnockdownHit" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_State_KnockdownHit);

	/** 공중 공격(Air) 상태 태그. 보스 체공 공격 구간 중 — MovementMode=Flying 유지. 문자열: "Character.Attack.Air" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_Attack_Air);
}
