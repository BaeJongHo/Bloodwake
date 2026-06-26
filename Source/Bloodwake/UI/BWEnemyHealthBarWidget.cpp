// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BWEnemyHealthBarWidget.h"

void UBWEnemyHealthBarWidget::SetHealthPercent(float Percent)
{
	// 0~1 범위로 클램프 후 BP 이벤트로 비율을 전달한다.
	const float ClampedPercent = FMath::Clamp(Percent, 0.f, 1.f);
	OnHealthPercentChanged(ClampedPercent);
}
