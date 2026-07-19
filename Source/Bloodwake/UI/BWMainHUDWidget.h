// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWMainHUDWidget.generated.h"

class UProgressBar;
class UBWAttributeComponent;
class UBWPotionInventoryComponent;
class UBWPotionWidget;
class UBWEquipmentWidget;
class UBWCombatComponent;
class ABWEquipItem;
enum class EBWEquipSlot : uint8;

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
	 * AttributeComponent / PotionInventoryComponent / CombatComponent를 구독하고 현재값으로 위젯을 초기화한다.
	 * ABWGameMode::InitializeHUD(다음 틱)에서 호출한다.
	 * 각 인자가 null이면 해당 구독만 건너뛴다(나머지 구독은 유지).
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void InitializeForPlayer(UBWAttributeComponent* InAttributes,
	                         UBWPotionInventoryComponent* InPotionInventory,
	                         UBWCombatComponent* InCombatComponent);

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

	/** 포션 수량 변경 시 델리게이트 콜백. PotionWidget->SetPotionQuantity 호출. */
	UFUNCTION()
	void OnPotionQuantityChanged(int32 InAmount);

	/**
	 * 장비 변경 델리게이트 콜백. 슬롯에 따라 해당 위젯의 아이콘을 갱신한다.
	 * UFUNCTION 필수 — AddDynamic 요구(없으면 조용히 바인딩 실패, CLAUDE.md 델리게이트 규약).
	 */
	UFUNCTION()
	void OnEquipmentChangedCallback(EBWEquipSlot InSlot, ABWEquipItem* NewItem);

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

	/** 포션 수량 위젯. WBP_MainHUD 자식에서 "PotionWidget" 이름으로 WBP_Potion 인스턴스를 배치해야 한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBWPotionWidget> PotionWidget;

	/**
	 * 무기 슬롯 아이콘 위젯. WBP_MainHUD 자식에서 이름이 정확히 "WeaponEquipmentWidget"인
	 * WBP_Equipment 인스턴스를 배치해야 한다(BindWidget 매칭).
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBWEquipmentWidget> WeaponEquipmentWidget;

	/**
	 * 방패 슬롯 아이콘 위젯. WBP_MainHUD 자식에서 이름이 정확히 "ShieldEquipmentWidget"인
	 * WBP_Equipment 인스턴스를 배치해야 한다(BindWidget 매칭).
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBWEquipmentWidget> ShieldEquipmentWidget;

	// ── 캐시 ────────────────────────────────────────────────────────

	/** 구독 중인 AttributeComponent 참조. NativeDestruct에서 RemoveDynamic 정리. */
	UPROPERTY(Transient)
	TObjectPtr<UBWAttributeComponent> BoundAttributes;

	/** 구독 중인 PotionInventoryComponent 참조. NativeDestruct에서 RemoveDynamic 정리. */
	UPROPERTY(Transient)
	TObjectPtr<UBWPotionInventoryComponent> BoundPotionInventory;

	/** 구독 중인 CombatComponent 참조. NativeDestruct에서 RemoveDynamic 정리(댕글링 콜백 방지). */
	UPROPERTY(Transient)
	TObjectPtr<UBWCombatComponent> BoundCombatComponent;
};
