// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "BWEnemyHealthBarComponent.generated.h"

class UBWAttributeComponent;
class UBWEnemyHealthBarWidget;

/**
 * 적 머리 위 Screen-space HP 바 위젯 컴포넌트.
 * ABWEnemy 생성자에서 CreateDefaultSubobject로 생성해 RootComponent에 부착한다.
 * BeginPlay에서 소유 적의 UBWAttributeComponent::OnHealthChanged를 구독하고,
 * 데미지/회복 시 HandleHealthChanged로 위젯 HP 비율을 갱신한다.
 * 평소 숨김 상태이며, ABWEnemyAIController가 감지 시 ShowBar(), 감지 해제 시 HideBar()를 호출한다.
 * EndPlay에서 델리게이트를 RemoveDynamic해 댕글링 콜백을 방지한다.
 * WidgetClass(WBP_EnemyHealthBar)는 BP 자식 BP_BWEnemy의 "(상속됨)" 컴포넌트에서 지정한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS(ClassGroup = (Bloodwake), meta = (BlueprintSpawnableComponent))
class BLOODWAKE_API UBWEnemyHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UBWEnemyHealthBarComponent();

	/** 컴포넌트를 화면에 표시한다. ABWEnemyAIController가 플레이어 감지 시 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "UI|HealthBar")
	void ShowBar();

	/** 컴포넌트를 숨긴다. ABWEnemyAIController가 감지 종료 시 호출한다. */
	UFUNCTION(BlueprintCallable, Category = "UI|HealthBar")
	void HideBar();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * UBWAttributeComponent::OnHealthChanged 델리게이트 콜백.
	 * 현재 HP 비율을 계산해 위젯에 전달한다.
	 * AddDynamic 대상이므로 UFUNCTION 필수.
	 */
	UFUNCTION()
	void HandleHealthChanged(float NewValue, float MaxValue);

private:
	/**
	 * 구독 중인 AttributeComponent 약참조.
	 * 소유하지 않으므로 TWeakObjectPtr 사용(CLAUDE.md 3.3).
	 * BeginPlay에서 한 번 캐시, EndPlay에서 RemoveDynamic 시 사용.
	 */
	TWeakObjectPtr<UBWAttributeComponent> CachedAttributeComponent;
};
