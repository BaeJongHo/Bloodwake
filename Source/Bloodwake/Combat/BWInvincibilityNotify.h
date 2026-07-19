// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "BWInvincibilityNotify.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

// IBWCombatInterface — 전방 선언 불가(UINTERFACE 구조체이므로). .cpp에서 include.
class IBWCombatInterface;

/**
 * 무적(i-frame) 유효 프레임 윈도우 AnimNotifyState.
 * 구르기 몽타주의 무적 구간에 배치한다(몽타주 에디터에서 "BW Invincibility Window"로 표시).
 * NotifyBegin: 소유 액터의 IBWCombatInterface::SetInvincible(true) 호출.
 * NotifyEnd  : 소유 액터의 IBWCombatInterface::SetInvincible(false) 호출.
 * 기존 UBWAnimNotifyState_BWParry 구조와 동일 패턴.
 *
 * ※ 이 클래스는 멤버 변수를 갖지 않는다.
 *    UAnimNotifyState 인스턴스는 몽타주 에셋에 공유되므로 런타임 상태를 담으면
 *    여러 캐릭터가 같은 몽타주를 재생할 때 상태가 오염된다.
 *    소유자는 MeshComp에서 역추적한다(NotifyBegin/End 인자 경유).
 */
UCLASS(meta = (DisplayName = "BW Invincibility Window"))
class BLOODWAKE_API UBWAnimNotifyState_Invincibility : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 소유 액터의 IBWCombatInterface::SetInvincible(true) 호출. */
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventRef) override;

	/** 소유 액터의 IBWCombatInterface::SetInvincible(false) 호출. */
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventRef) override;

	virtual FString GetNotifyName_Implementation() const override;

private:
	/**
	 * MeshComp 소유 액터를 IBWCombatInterface로 변환해 반환한다.
	 * 찾지 못하면 Warning 로그 후 nullptr. UBWAnimNotifyState_BWParry::FindStateComponent 패턴 동일.
	 */
	static IBWCombatInterface* FindCombatInterface(const USkeletalMeshComponent* MeshComp);
};
