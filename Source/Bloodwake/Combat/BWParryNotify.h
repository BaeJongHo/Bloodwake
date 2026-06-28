// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "BWParryNotify.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;
class UBWStateComponent;

/**
 * 패링 유효 프레임 윈도우 NotifyState.
 * 패링 몽타주의 "막기 성공 인정 구간"에 배치한다.
 * NotifyBegin: 소유 캐릭터에 Character.State.Parrying 태그를 부착한다.
 * NotifyEnd  : 해당 태그를 해제한다.
 * Blocking 시스템의 UBWComboAttackWindowNotifyState 구조를 그대로 따른다.
 */
UCLASS(meta = (DisplayName = "BW Parry Window"))
class BLOODWAKE_API UBWAnimNotifyState_BWParry : public UAnimNotifyState
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
	 * MeshComp 소유 액터에서 UBWStateComponent를 조회한다.
	 * 찾지 못하면 Warning 로그를 남기고 nullptr를 반환한다.
	 * UBWComboAttackWindowNotifyState의 FindAttackComponent 패턴을 그대로 따른다.
	 */
	static UBWStateComponent* FindStateComponent(const USkeletalMeshComponent* MeshComp);
};
