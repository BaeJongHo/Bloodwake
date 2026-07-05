// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BWMainHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Combat/BWAttributeComponent.h"
#include "Combat/BWPotionInventoryComponent.h"
#include "UI/BWPotionWidget.h"

void UBWMainHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// AttributeComponent는 InitializeForPlayer에서 연결한다.
	// 소유 캐릭터가 늦게 들어오는 경우 GameMode가 준비 후 호출하므로 여기서는 안전 가드만.
}

void UBWMainHUDWidget::NativeDestruct()
{
	if (IsValid(BoundAttributes))
	{
		BoundAttributes->OnHealthChanged.RemoveDynamic(this, &UBWMainHUDWidget::OnHealthChangedCallback);
		BoundAttributes->OnStaminaChanged.RemoveDynamic(this, &UBWMainHUDWidget::OnStaminaChangedCallback);
		BoundAttributes->OnFocusChanged.RemoveDynamic(this, &UBWMainHUDWidget::OnFocusChangedCallback);
		BoundAttributes = nullptr;
	}

	// 포션 델리게이트 구독 해제 (댕글링 콜백 방지 — CLAUDE.md 4.3)
	if (IsValid(BoundPotionInventory))
	{
		BoundPotionInventory->OnUpdatePotionAmount.RemoveDynamic(this, &UBWMainHUDWidget::OnPotionQuantityChanged);
		BoundPotionInventory = nullptr;
	}

	Super::NativeDestruct();
}

void UBWMainHUDWidget::InitializeForPlayer(UBWAttributeComponent* InAttributes, UBWPotionInventoryComponent* InPotionInventory)
{
	if (!IsValid(InAttributes))
	{
		UE_LOG(LogTemp, Warning, TEXT("UBWMainHUDWidget::InitializeForPlayer — InAttributes가 유효하지 않습니다."));
		return;
	}

	// 이전 Attribute 구독이 있으면 먼저 해제
	if (IsValid(BoundAttributes))
	{
		BoundAttributes->OnHealthChanged.RemoveDynamic(this, &UBWMainHUDWidget::OnHealthChangedCallback);
		BoundAttributes->OnStaminaChanged.RemoveDynamic(this, &UBWMainHUDWidget::OnStaminaChangedCallback);
		BoundAttributes->OnFocusChanged.RemoveDynamic(this, &UBWMainHUDWidget::OnFocusChangedCallback);
	}

	BoundAttributes = InAttributes;

	BoundAttributes->OnHealthChanged.AddDynamic(this, &UBWMainHUDWidget::OnHealthChangedCallback);
	BoundAttributes->OnStaminaChanged.AddDynamic(this, &UBWMainHUDWidget::OnStaminaChangedCallback);
	BoundAttributes->OnFocusChanged.AddDynamic(this, &UBWMainHUDWidget::OnFocusChangedCallback);

	// 현재값으로 즉시 갱신
	SetBarPercent(HealthBar, BoundAttributes->GetHealth(), BoundAttributes->GetMaxHealth());
	SetBarPercent(StaminaBar, BoundAttributes->GetStamina(), BoundAttributes->GetMaxStamina());
	SetBarPercent(FocusBar, BoundAttributes->GetFocus(), BoundAttributes->GetMaxFocus());

	// ── 포션 인벤토리 구독 ──────────────────────────────────────────────
	// 이전 포션 구독이 있으면 먼저 해제
	if (IsValid(BoundPotionInventory))
	{
		BoundPotionInventory->OnUpdatePotionAmount.RemoveDynamic(this, &UBWMainHUDWidget::OnPotionQuantityChanged);
		BoundPotionInventory = nullptr;
	}

	if (IsValid(InPotionInventory))
	{
		BoundPotionInventory = InPotionInventory;
		BoundPotionInventory->OnUpdatePotionAmount.AddDynamic(this, &UBWMainHUDWidget::OnPotionQuantityChanged);

		// 구독 직후 현재값으로 초기 표시를 보장한다(컴포넌트 BeginPlay 브로드캐스트 타이밍에 의존하지 않음).
		OnPotionQuantityChanged(BoundPotionInventory->GetPotionQuantity());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UBWMainHUDWidget::InitializeForPlayer — InPotionInventory가 유효하지 않습니다. 포션 HUD가 갱신되지 않습니다."));
	}
}

void UBWMainHUDWidget::OnHealthChangedCallback(float NewValue, float MaxValue)
{
	SetBarPercent(HealthBar, NewValue, MaxValue);
}

void UBWMainHUDWidget::OnStaminaChangedCallback(float NewValue, float MaxValue)
{
	SetBarPercent(StaminaBar, NewValue, MaxValue);
}

void UBWMainHUDWidget::OnFocusChangedCallback(float NewValue, float MaxValue)
{
	SetBarPercent(FocusBar, NewValue, MaxValue);
}

void UBWMainHUDWidget::OnPotionQuantityChanged(int32 InAmount)
{
	if (!IsValid(PotionWidget))
	{
		return;
	}

	PotionWidget->SetPotionQuantity(InAmount);
}

void UBWMainHUDWidget::SetBarPercent(UProgressBar* Bar, float Value, float Max)
{
	if (!IsValid(Bar))
	{
		return;
	}

	const float Percent = (Max > 0.f) ? FMath::Clamp(Value / Max, 0.f, 1.f) : 0.f;
	Bar->SetPercent(Percent);
}
