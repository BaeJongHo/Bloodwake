// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "BWRotateToTargetNotify.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

/**
 * 보스 공격 windup 동안 소유 폰을 타깃(플레이어) 방향으로 yaw 보간 회전시키는 AnimNotifyState.
 * NotifyTick 매 틱마다 AIController->GetCurrentTarget()으로 라이브 타깃을 조회해 회전을 보간한다.
 * bOrientRotationToMovement 회전(이동 방향 정렬)과 충돌하지 않도록 windup(정지 상태) 구간에만 배치한다.
 *
 * 네이밍 계승: UBWAnimNotifyState_BWParry, UBWAnimNotifyState_WeaponCollision — BW 태그 적용.
 * 위치: Combat/ (기존 노티파이 관례 동일).
 *
 * ※ AnimNotifyState는 에셋 내 공유 인스턴스이므로 per-instance 상태를 멤버에 저장하지 않는다.
 *    매 NotifyTick에서 라이브 조회로 처리한다(공유 인스턴스 안전 패턴).
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS(meta = (DisplayName = "BW Rotate To Target"))
class BLOODWAKE_API UBWAnimNotifyState_RotateToTarget : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/**
	 * NotifyState 구간 동안 매 틱 호출된다.
	 * 소유 폰의 yaw를 타깃 방향으로 RInterpTo 보간 회전한다.
	 * 타깃이 없거나(AIController 미설정, 에디터 프리뷰 등) 방향 벡터가 0이면 무동작.
	 */
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	/** BT 에디터·노티파이 트랙에 표시될 이름. */
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/**
	 * yaw 보간 속도(RInterpTo RotationInterpSpeed 파라미터).
	 * 클수록 타깃을 빠르게 바라본다. 애니메이터가 몽타주 트랙 디테일에서 windup 길이에 맞춰 튜닝한다.
	 * BP 자식에서도 조정 가능.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Rotation", meta = (ClampMin = "0.0"))
	float RotationInterpSpeed = 8.f;

private:
	/**
	 * MeshComp 소유 폰의 AIController에서 현재 추격 타깃을 반환한다.
	 * ABWEnemyAIController 이외의 컨트롤러(에디터 프리뷰, nullptr 등)면 nullptr 반환 — 안전.
	 */
	static AActor* ResolveTarget(const USkeletalMeshComponent* MeshComp);
};
