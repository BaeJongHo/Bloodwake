// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/BWStateComponent.h"

#include "Core/BWGameplayDefine.h"

UBWStateComponent::UBWStateComponent()
{
	// 상태 변경은 이벤트 기반으로 처리한다. 매 프레임 틱은 비활성.
	PrimaryComponentTick.bCanEverTick = false;
}

void UBWStateComponent::BeginPlay()
{
	Super::BeginPlay();

	// 행동 상태가 없는 기본 상태(Normal)로 시작한다.
	AddStateTag(BWGameplayTags::Character_State_Normal.GetTag());
}

void UBWStateComponent::AddStateTag(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}

	// 이미 존재하면 무시(중복 브로드캐스트 방지).
	if (ActiveStateTags.HasTagExact(Tag))
	{
		return;
	}

	ActiveStateTags.AddTag(Tag);
	OnStateTagChanged.Broadcast(Tag, /*bAdded=*/true);

	// 행동 상태 태그가 추가되면 기본 상태(Normal)를 자동 해제한다.
	const FGameplayTag NormalTag = BWGameplayTags::Character_State_Normal.GetTag();
	if (Tag != NormalTag && ActiveStateTags.HasTagExact(NormalTag))
	{
		RemoveStateTag(NormalTag);
	}
}

void UBWStateComponent::RemoveStateTag(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}

	// 존재하지 않으면 무시.
	if (!ActiveStateTags.HasTagExact(Tag))
	{
		return;
	}

	ActiveStateTags.RemoveTag(Tag);
	OnStateTagChanged.Broadcast(Tag, /*bAdded=*/false);

	// 행동 상태 태그가 모두 비면 기본 상태(Normal)로 복귀한다.
	const FGameplayTag NormalTag = BWGameplayTags::Character_State_Normal.GetTag();
	if (Tag != NormalTag && !HasAnyActionState())
	{
		AddStateTag(NormalTag);
	}
}

bool UBWStateComponent::HasAnyActionState() const
{
	// Normal(기본 상태)을 제외한 태그가 하나라도 있으면 true.
	const FGameplayTag NormalTag = BWGameplayTags::Character_State_Normal.GetTag();
	for (const FGameplayTag& Tag : ActiveStateTags)
	{
		if (Tag != NormalTag)
		{
			return true;
		}
	}
	return false;
}

bool UBWStateComponent::HasStateTag(FGameplayTag Tag) const
{
	return ActiveStateTags.HasTagExact(Tag);
}

bool UBWStateComponent::HasAnyStateTag(const FGameplayTagContainer& Tags) const
{
	// HasAny: Tags 중 하나라도 매칭(부모 태그 포함)되면 true.
	return ActiveStateTags.HasAny(Tags);
}

void UBWStateComponent::ClearAllStateTags()
{
	// 사망·리셋 시 모든 상태를 비운다(Normal 자동 복귀를 우회해 완전히 비운 상태로 둔다).
	// RemoveStateTag를 거치지 않고 직접 제거 후 개별 브로드캐스트한다.
	FGameplayTagContainer TagsToRemove = ActiveStateTags;
	for (const FGameplayTag& Tag : TagsToRemove)
	{
		ActiveStateTags.RemoveTag(Tag);
		OnStateTagChanged.Broadcast(Tag, /*bAdded=*/false);
	}
}
