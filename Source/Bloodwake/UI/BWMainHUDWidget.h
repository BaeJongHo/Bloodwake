// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWMainHUDWidget.generated.h"

class UProgressBar;
class UBWAttributeComponent;

/**
 * 플레이어 메인 HUD 위젯의 C++ 베이스 클래스.
 * Health / Stamina / Focus ProgressBar 3개를 AttributeComponent 델리게이트에 구독시켜 갱신한다.
 * 레이아웃·색·위치는 WBP_MainHUD(BP 자식)에서 구성하며, 동일 이름(HealthBar/StaminaBar/FocusBar)의
 * ProgressBar 위젯을 반드시 배치해야 한다(BindWidget 매칭).
 */
UCLASS(Blueprintable)
class BLOODWAKE_API UBWMainHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * AttributeComponent를 구독하고 현재값으로 ProgressBar를 초기화한다.
	 * ABWGameMode::BeginPlay에서 HUD 생성 직후 호출한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void InitializeForPlayer(UBWAttributeComponent* InAttributes);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ── AttributeComponent 델리게이트 콜백 ──────────────────────────

	UFUNCTION()
	void OnHealthChangedCallback(float NewValue, float MaxValue);

	UFUNCTION()
	void OnStaminaChangedCallback(float NewValue, float MaxValue);

	UFUNCTION()
	void OnFocusChangedCallback(float NewValue, float MaxValue);

private:
	/** ProgressBar의 퍼센트를 설정한다. MaxValue == 0이면 0으로 세팅(0-division 가드). */
	void SetBarPercent(UProgressBar* Bar, float Value, float Max);

	// ── BindWidget (WBP 자식에서 동일 이름으로 배치 필수) ───────────

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> StaminaBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> FocusBar;

	// ── 캐시 ────────────────────────────────────────────────────────

	/** 구독 중인 AttributeComponent 참조. NativeDestruct에서 RemoveDynamic 정리. */
	UPROPERTY(Transient)
	TObjectPtr<UBWAttributeComponent> BoundAttributes;
};
