// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWEnemyHealthBarWidget.generated.h"

/**
 * 적 머리 위 HP 바 위젯의 C++ 베이스 클래스.
 * 로직(SetHealthPercent)은 C++에서, 표시(ProgressBar SetPercent)는 WBP_EnemyHealthBar(BP 자식)에서 담당한다.
 * WBP_EnemyHealthBar는 이 클래스를 부모로 두고 OnHealthPercentChanged 이벤트에서 ProgressBar를 갱신한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS(Blueprintable)
class BLOODWAKE_API UBWEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * HP 비율(0~1)을 위젯에 반영한다.
	 * UBWEnemyHealthBarComponent::HandleHealthChanged에서 호출한다.
	 * BP 자식 OnHealthPercentChanged 이벤트로 ProgressBar.SetPercent를 연결한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|HealthBar")
	void SetHealthPercent(float Percent);

protected:
	/**
	 * BP 자식에서 구현하는 이벤트.
	 * WBP_EnemyHealthBar의 OnHealthPercentChanged 이벤트 그래프에서 ProgressBar SetPercent를 호출한다.
	 * 로직 없음 — 표시만.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "UI|HealthBar")
	void OnHealthPercentChanged(float Percent);
};
