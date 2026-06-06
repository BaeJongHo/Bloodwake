// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BWStateComponent.generated.h"

// 상태 태그 추가/제거 시 브로드캐스트 (변경된 태그, 추가=true/제거=false)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBWOnStateTagChanged, FGameplayTag, ChangedTag, bool, bAdded);

/**
 * 캐릭터의 현재 행동 상태(Sprint / Roll / Attack 등)를 GameplayTagContainer로 보유하는 컴포넌트.
 * 상태 추가/제거/질의 API와 변경 알림 델리게이트를 제공한다.
 * 1단계: GAS 미적용. 순수 상태 보관소이며 튜닝 데이터를 포함하지 않는다.
 * Tick 미사용. 플레이어/적/보스 재사용 가능.
 *
 * Normal(기본 상태) 자동 관리: BeginPlay에서 Character.State.Normal을 부여하고,
 * 행동 상태 태그가 추가되면 Normal을 자동 해제, 행동 상태가 모두 비면 Normal을 자동 복귀시킨다.
 * 따라서 항상 "Normal" 또는 "하나 이상의 행동 상태" 중 하나가 성립한다(ClearAllStateTags 직후 제외).
 */
UCLASS(Blueprintable, ClassGroup = (Bloodwake), meta = (BlueprintSpawnableComponent))
class BLOODWAKE_API UBWStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBWStateComponent();

	// ── 상태 태그 API ────────────────────────────────────────────────

	/**
	 * 상태 태그를 추가한다. 이미 존재하면 무시한다.
	 * 추가되면 OnStateTagChanged(Tag, true) 브로드캐스트.
	 */
	UFUNCTION(BlueprintCallable, Category = "State")
	void AddStateTag(FGameplayTag Tag);

	/**
	 * 상태 태그를 제거한다. 존재하지 않으면 무시한다.
	 * 제거되면 OnStateTagChanged(Tag, false) 브로드캐스트.
	 */
	UFUNCTION(BlueprintCallable, Category = "State")
	void RemoveStateTag(FGameplayTag Tag);

	/** 정확히 해당 태그를 보유하는지 확인한다. */
	UFUNCTION(BlueprintPure, Category = "State")
	bool HasStateTag(FGameplayTag Tag) const;

	/**
	 * Tags 중 하나라도 보유하는지 확인한다.
	 * 부모 태그 매칭 포함(예: "Character.State"로 하위 전체 확인 가능).
	 */
	UFUNCTION(BlueprintPure, Category = "State")
	bool HasAnyStateTag(const FGameplayTagContainer& Tags) const;

	/** 현재 활성 상태 컨테이너의 사본을 반환한다. */
	UFUNCTION(BlueprintPure, Category = "State")
	FGameplayTagContainer GetActiveStateTags() const { return ActiveStateTags; }

	/**
	 * 모든 상태 태그를 해제한다(사망·리셋 시 사용).
	 * 제거된 각 태그에 대해 OnStateTagChanged(Tag, false) 브로드캐스트.
	 */
	UFUNCTION(BlueprintCallable, Category = "State")
	void ClearAllStateTags();

	// ── 델리게이트 ───────────────────────────────────────────────────

	/** 상태 태그가 추가/제거될 때 브로드캐스트. AnimInstance·HUD 등이 구독한다. */
	UPROPERTY(BlueprintAssignable, Category = "State")
	FBWOnStateTagChanged OnStateTagChanged;

protected:
	// 기본 상태(Normal)를 부여한다.
	virtual void BeginPlay() override;

	/**
	 * Normal(기본 상태)을 제외한 행동 상태 태그가 하나라도 있는지 확인한다.
	 * Normal 자동 복귀 판정에 사용한다.
	 */
	bool HasAnyActionState() const;

	// ── 활성 상태 태그 ───────────────────────────────────────────────

	/** 현재 활성 상태 태그 집합. 디버그 가시화용으로만 노출(쓰기는 API 경유). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	FGameplayTagContainer ActiveStateTags;
};
