// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "BWComboAttackNotify.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

/**
 * 콤보 입력 윈도우 NotifyState.
 * 애니메이터가 몽타주 트랙에 "다음 콤보 입력을 받을 구간"으로 배치한다.
 * Begin → OpenComboWindow(), End → CloseComboWindow() 호출.
 */
UCLASS(meta = (DisplayName = "BW Combo Attack Window"))
class BLOODWAKE_API UBWComboAttackWindowNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventRef) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventRef) override;

	virtual FString GetNotifyName_Implementation() const override;
};

/**
 * 콤보 종료 Notify.
 * 콤보 마지막 단계 몽타주 끝부분(또는 콤보를 끝내고 싶은 지점)에 배치한다.
 * Notify → NotifyComboEnd() 호출.
 */
UCLASS(meta = (DisplayName = "BW Combo Attack End"))
class BLOODWAKE_API UBWComboAttackEndNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventRef) override;

	virtual FString GetNotifyName_Implementation() const override;
};
