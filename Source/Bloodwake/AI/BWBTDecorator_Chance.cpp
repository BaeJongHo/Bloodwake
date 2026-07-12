// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/BWBTDecorator_Chance.h"

UBWBTDecorator_Chance::UBWBTDecorator_Chance()
{
	// BT 에디터에 표시될 노드 이름.
	NodeName = TEXT("BW Chance");

	// 순수 확률 게이트 — 관찰자 중단/틱이 필요 없다(1회성 롤). FlowAbortMode 기본값(None) 유지.
}

bool UBWBTDecorator_Chance::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	// 경계값은 결정적으로 처리한다(난수의 [0,1] 경계 포함 문제 방지).
	if (Chance >= 1.f)
	{
		return true;
	}
	if (Chance <= 0.f)
	{
		return false;
	}

	// FMath::FRand()는 [0,1] 범위. Chance보다 작으면 통과.
	// 예: Chance = 0.4 → 난수가 0.4 미만일 확률 = 40%.
	return FMath::FRand() < Chance;
}

FString UBWBTDecorator_Chance::GetStaticDescription() const
{
	return FString::Printf(TEXT("Chance: %.0f%%"), Chance * 100.f);
}
