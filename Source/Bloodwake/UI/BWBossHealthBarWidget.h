// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWBossHealthBarWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UBWAttributeComponent;

/**
 * 화면 상단 2D 스크린 스페이스 보스 HP 바 위젯의 C++ 베이스 클래스.
 * 다크소울·엘든링식 뷰포트 상단 고정 바 — AddToViewport로 추가되며 월드 공간에 부착되지 않는다.
 * 보스 이름 TextBlock(BossNameText) + HP ProgressBar(HealthBar)를 BindWidget으로 연결한다.
 * 레이아웃·색·폰트·장식은 WBP_BossHealthBar(BP 자식)에서 구성한다.
 *
 * 패턴 계승: UBWMainHUDWidget — BindWidget + Attribute 델리게이트 구독 + NativeDestruct 정리.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS(Blueprintable)
class BLOODWAKE_API UBWBossHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * AttributeComponent 구독 + 보스 이름 설정 + 현재값으로 즉시 초기화.
	 * ABWEnemy_Boss::EnsureBossHealthBar에서 AddToViewport 직후 호출한다.
	 * 중복 호출 안전 — 이전 구독이 있으면 먼저 해제 후 재구독한다.
	 * @param InAttributes 구독할 ABWEnemy_Boss의 UBWAttributeComponent.
	 * @param InBossName   화면에 표시할 보스 이름 텍스트.
	 */
	UFUNCTION(BlueprintCallable, Category = "UI|BossHealthBar")
	void InitializeForBoss(UBWAttributeComponent* InAttributes, const FText& InBossName);

protected:
	/** 위젯이 파괴될 때 OnHealthChanged 구독을 해제해 댕글링 콜백을 방지한다(CLAUDE.md 4.3). */
	virtual void NativeDestruct() override;

	/**
	 * UBWAttributeComponent::OnHealthChanged 델리게이트 콜백.
	 * HealthBar의 SetPercent를 NewValue/MaxValue 비율로 갱신한다.
	 * AddDynamic 바인딩 대상이므로 UFUNCTION 필수.
	 */
	UFUNCTION()
	void OnHealthChangedCallback(float NewValue, float MaxValue);

private:
	// ── BindWidget (WBP 자식에서 동일 이름으로 배치 필수) ───────────────────────

	/** 보스 이름 텍스트. WBP 자식에 이름 "BossNameText"인 TextBlock을 반드시 배치해야 한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BossNameText;

	/** 보스 HP ProgressBar. WBP 자식에 이름 "HealthBar"인 ProgressBar를 반드시 배치해야 한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	// ── 캐시 ────────────────────────────────────────────────────────────────────

	/** 구독 중인 AttributeComponent. NativeDestruct에서 RemoveDynamic 정리. GC 추적 필수. */
	UPROPERTY(Transient)
	TObjectPtr<UBWAttributeComponent> BoundAttributes;
};
