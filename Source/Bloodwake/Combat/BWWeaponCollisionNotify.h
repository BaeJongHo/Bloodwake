// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "BWWeaponCollisionNotify.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

/**
 * 공격 몽타주 트랙의 히트 윈도우 구간을 나타내는 AnimNotifyState.
 * Begin → UBWWeaponCollisionComponent::StartDetection(),
 * End   → UBWWeaponCollisionComponent::StopDetection() 호출.
 * 로직은 컴포넌트에 완전히 위임한다 — 이 클래스는 트리거 역할만 한다.
 * 애니메이터가 몽타주 에디터에서 "BW Weapon Collision Window"로 표시해 배치한다.
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
};
