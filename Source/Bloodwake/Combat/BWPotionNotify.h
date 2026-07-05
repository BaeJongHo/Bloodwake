// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "BWPotionNotify.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;
class UBWPotionInventoryComponent;

/**
 * 포션 마시기 몽타주의 "손에 포션이 들려 있어야 하는" 구간에 배치하는 NotifyState.
 * NotifyBegin: 소유 캐릭터의 UBWPotionInventoryComponent->SpawnPotion()을 호출한다.
 * NotifyEnd  : DespawnPotion()을 호출해 포션 액터를 정리한다.
 * BWParryNotify의 Begin/End 구조와 동일 패턴.
 */
UCLASS(meta = (DisplayName = "BW Potion Spawn"))
class BLOODWAKE_API UBWAnimNotifyState_BWPotionSpawn : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventRef) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventRef) override;

	virtual FString GetNotifyName_Implementation() const override;

private:
	/**
	 * MeshComp 소유 액터에서 UBWPotionInventoryComponent를 조회한다.
	 * 찾지 못하면 Warning 로그를 남기고 nullptr을 반환한다.
	 * BWParryNotify의 FindStateComponent 패턴을 그대로 따른다.
	 */
	static UBWPotionInventoryComponent* FindPotionInventory(const USkeletalMeshComponent* MeshComp);
};

/**
 * 포션 마시기 몽타주에서 실제 회복이 발생해야 하는 단발 타이밍에 배치하는 Notify.
 * Notify: 소유 캐릭터의 UBWPotionInventoryComponent->DrinkPotion()을 1회 호출한다(체력 회복 + 수량 감소).
 */
UCLASS(meta = (DisplayName = "BW Potion Drink"))
class BLOODWAKE_API UBWAnimNotify_BWPotionDrink : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

private:
	static UBWPotionInventoryComponent* FindPotionInventory(const USkeletalMeshComponent* MeshComp);
};
