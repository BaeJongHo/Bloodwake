// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "BWFlyingNotify.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;
class ACharacter;
class UBWStateComponent;

/**
 * 보스 공중 공격 몽타주의 체공 구간에 배치하는 NotifyState.
 * NotifyBegin: MovementMode를 MOVE_Flying으로 전환 + Character.Attack.Air 태그 부착.
 * NotifyEnd  : MovementMode를 MOVE_Walking으로 복귀 + Character.Attack.Air 태그 해제.
 * 태그·MovementMode 전환이 Begin/End로 정확히 대칭된다(UBWAnimNotifyState_BWParry와 동일 구조).
 *
 * 에디터 애님 프리뷰(컨트롤러/월드 없음)에서도 크래시가 없도록
 * 모든 조회는 IsValid 가드 후 조용히 return 한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS(meta = (DisplayName = "BW Flying (Air Attack)"))
class BLOODWAKE_API UBWAnimNotifyState_BWFlying : public UAnimNotifyState
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
	 * MeshComp 소유 액터에서 ACharacter를 조회한다.
	 * null 또는 ACharacter 캐스트 실패 시 nullptr 반환(Warning 없음 — 프리뷰 안전).
	 */
	static ACharacter* FindOwnerCharacter(const USkeletalMeshComponent* MeshComp);

	/**
	 * MeshComp 소유 액터에서 UBWStateComponent를 조회한다.
	 * 찾지 못하면 Warning 로그를 남기고 nullptr 반환.
	 * UBWAnimNotifyState_BWParry::FindStateComponent와 동일 패턴.
	 */
	static UBWStateComponent* FindStateComponent(const USkeletalMeshComponent* MeshComp);
};
