// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/BWGameplayDefine.h"

namespace BWGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Normal, "Character.State.Normal", "기본(Normal) 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Sprint, "Character.State.Sprint", "질주(Sprint) 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Roll,   "Character.State.Roll",   "구르기(Roll) 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Death, "Character.State.Death",   "죽은(Death) 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Hit,   "Character.State.Hit",     "피격(Hit) 경직 상태");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Action_Equip,   "Character.Action.Equip",   "장착(무기/방패 뽑기) 행동 진행 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Action_Unequip, "Character.Action.Unequip", "해제(무기/방패 넣기) 행동 진행 중");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Attack_Light,   "Character.Attack.Light",   "약공격(콤보) 진행 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Attack_Running, "Character.Attack.Running", "대쉬 공격(스프린트 중 공격) 진행 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Attack_Special, "Character.Attack.Special", "특수 공격(길게 누름) 진행 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Attack_Heavy,   "Character.Attack.Heavy",   "강공격(콤보) 진행 중");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Blocking,      "Character.State.Blocking",      "방패 블로킹(가드) 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Action_BlockingHit,  "Character.Action.BlockingHit",  "가드 성공 후 히트 리액션 진행 중");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Parrying,      "Character.State.Parrying",      "패링 유효 윈도우 진행 중 — AnimNotifyState가 토글");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Parried,       "Character.State.Parried",       "적이 패링 당함 경직 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_Action_ParriedHit,   "Character.Action.ParriedHit",   "패링 당함 리액션 몽타주 진행 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Character_State_Stunned,       "Character.State.Stunned",       "적 스턴 상태 — 행동 불능");
}
