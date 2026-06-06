// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BWMainHUDWidget.h"

#include "Components/ProgressBar.h"
#include "Combat/BWAttributeComponent.h"

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

	Super::NativeDestruct();
}

void UBWMainHUDWidget::InitializeForPlayer(UBWAttributeComponent* InAttributes)
{
	if (!IsValid(InAttributes))
	{
		UE_LOG(LogTemp, Warning, TEXT("UBWMainHUDWidget::InitializeForPlayer — InAttributes가 유효하지 않습니다."));
		return;
	}

	// 이전 구독이 있으면 먼저 해제
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

void UBWMainHUDWidget::SetBarPercent(UProgressBar* Bar, float Value, float Max)
{
	if (!IsValid(Bar))
	{
		return;
	}

	const float Percent = (Max > 0.f) ? FMath::Clamp(Value / Max, 0.f, 1.f) : 0.f;
	Bar->SetPercent(Percent);
}
