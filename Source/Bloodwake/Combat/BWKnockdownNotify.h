// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "BWKnockdownNotify.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

/**
 * 보스 공격 몽타주의 임팩트 프레임에 배치하는 넉다운 히트 노티파이.
 * 지정 소켓 위치 기준 구(sphere) 범위 내 Pawn을 오버랩해 소유자를 제외한
 * 대상에게 UBWDamageType_Knockdown 타입의 FPointDamageEvent 데미지를 1회 적용한다.
 * 데미지를 받은 ABWPlayerCharacter는 TakeDamage에서 넉다운 리액션 경로로 분기한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS(meta = (DisplayName = "BW Knockdown"))
class BLOODWAKE_API UBWAnimNotify_BWKnockdown : public UAnimNotify
{
	GENERATED_BODY()

public:
	UBWAnimNotify_BWKnockdown();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	// ── 노티파이 인스턴스별 튜닝 파라미터 (몽타주 트랙에서 설정) ─────────────

	/** 적용할 데미지 양. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Knockdown", meta = (ClampMin = "0.0"))
	float Damage = 30.f;

	/**
	 * 구 오버랩의 중심이 될 소켓 이름.
	 * NAME_None이면 소유 액터(보스) 위치를 폴백으로 사용하고 Warning을 남긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Knockdown")
	FName SocketName = NAME_None;

	/** 구 오버랩 반경(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Knockdown", meta = (ClampMin = "0.0"))
	float Radius = 200.f;

	/**
	 * 오버랩 탐지 채널.
	 * 무기 콜리전 컴포넌트(BWWeaponCollisionComponent)와 동일 채널 정책(기본 ECC_Pawn).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Knockdown")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	/**
	 * 적용할 데미지 타입 클래스.
	 * 생성자에서 UBWDamageType_Knockdown::StaticClass()로 초기화된다.
	 * 애니메이터가 다른 넉다운 변형 타입으로 교체할 수 있도록 EditAnywhere로 노출한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Knockdown")
	TSubclassOf<UDamageType> DamageTypeClass;

	/** true이면 오버랩 구 범위를 뷰포트에 1초간 표시한다(디버그용). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Knockdown")
	bool bDrawDebug = false;
};
