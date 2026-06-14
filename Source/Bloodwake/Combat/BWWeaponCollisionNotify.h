// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Combat/BWCombatTypes.h"
#include "BWWeaponCollisionNotify.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;
class UBWCombatComponent;

/**
 * 공격 몽타주 트랙의 히트 윈도우 구간을 나타내는 AnimNotifyState.
 * Begin → UBWWeaponCollisionComponent::StartDetection(),
 * End   → UBWWeaponCollisionComponent::StopDetection() 호출.
 * 로직은 UBWCombatComponent → GetCollisionForSlot(TargetSlot)로 조회한 컴포넌트에 완전히 위임한다.
 * 애니메이터가 몽타주 에디터에서 "BW Weapon Collision Window"로 표시해 배치하며,
 * TargetSlot(Main/Second)을 디테일 패널에서 지정한다.
 */
UCLASS(meta = (DisplayName = "BW Weapon Collision Window"))
class BLOODWAKE_API UBWAnimNotifyState_WeaponCollision : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventRef) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventRef) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/**
	 * 이 히트 윈도우가 제어할 콜리전 슬롯.
	 * 무기 전투: Main = 주무기.
	 * 맨손 전투: 오른손=Main, 왼손=Second(몽타주 트랙에서 각각 지정).
	 * 디테일 트랙에서 지정한다(기본값=Main).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponCollision")
	EBWWeaponSlotSelector TargetSlot = EBWWeaponSlotSelector::Main;

private:
	/**
	 * MeshComp 소유 액터에서 UBWCombatComponent를 찾아 반환하는 로컬 헬퍼.
	 * 컴포넌트를 찾지 못하면 Warning 로그를 남기고 nullptr를 반환한다.
	 */
	static UBWCombatComponent* FindCombatComponent(const USkeletalMeshComponent* MeshComp);
};
