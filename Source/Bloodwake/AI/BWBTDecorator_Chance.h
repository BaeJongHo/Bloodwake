// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "BWBTDecorator_Chance.generated.h"

/**
 * 확률 기반 게이트 데코레이터.
 * 설정한 Chance 확률로 조건을 통과(true)시킨다. 예: Chance = 0.4 → 40% 확률로 통과.
 * 브랜치가 평가될 때 [0,1) 난수를 1회 뽑아 Chance와 비교한다.
 *
 * 보스/적 패턴에서 "일정 확률로만 특정 공격·행동 분기 진입"을 만들 때 사용한다.
 * 부모 UBTDecorator의 Inverse Condition(Not) 옵션으로 결과 반전도 가능하다.
 *
 * 주의: 관찰자 중단(Observer Abort)을 켜면 조건이 반복 평가되며 매번 재-롤되어
 * 결과가 흔들린다. 확률 게이트는 기본값(Flow Control = None)으로 두고 1회성 롤로 쓰는 것을 권장한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS()
class BLOODWAKE_API UBWBTDecorator_Chance : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBWBTDecorator_Chance();

protected:
	/**
	 * 조건 통과 확률. 0.0~1.0 범위 (0.4 = 40%). BT 노드 디테일 패널에서 지정한다.
	 * 0 이하는 항상 실패, 1 이상은 항상 성공으로 처리한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Chance",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Chance = 0.5f;

	/** 확률에 따라 조건 통과 여부를 반환한다. 부모가 Inverse Condition을 적용한다. */
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

	/** BT 에디터 노드에 확률(%)을 표시한다. */
	virtual FString GetStaticDescription() const override;
};
