// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

// 캐릭터 행동 상태 네이티브 GameplayTag 선언.
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

	/** 공격(Attack) 상태 태그. 문자열: "Character.State.Attack" */
	BLOODWAKE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Character_State_Attack);
}
