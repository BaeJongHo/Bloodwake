// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BWBossHealthBarWidget.h"

#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Combat/BWAttributeComponent.h"

void UBWBossHealthBarWidget::InitializeForBoss(UBWAttributeComponent* InAttributes, const FText& InBossName)
{
	if (!IsValid(InAttributes))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[UBWBossHealthBarWidget] InitializeForBoss: InAttributes가 유효하지 않습니다."));
		return;
	}

	// 기존 구독 정리 — 중복 바인딩 방지 (CLAUDE.md 4.3)
	if (IsValid(BoundAttributes))
	{
		BoundAttributes->OnHealthChanged.RemoveDynamic(this, &UBWBossHealthBarWidget::OnHealthChangedCallback);
	}

	BoundAttributes = InAttributes;
	BoundAttributes->OnHealthChanged.AddDynamic(this, &UBWBossHealthBarWidget::OnHealthChangedCallback);

	// 보스 이름 설정
	if (BossNameText)
	{
		BossNameText->SetText(InBossName);
	}

	// 현재 체력으로 즉시 초기화 — 0% 깜빡임 방지 (UBWEnemyHealthBarComponent::BeginPlay 선례 동일)
	const float MaxHealth = BoundAttributes->GetMaxHealth();
	if (MaxHealth > 0.f)
	{
		OnHealthChangedCallback(BoundAttributes->GetHealth(), MaxHealth);
	}
}

void UBWBossHealthBarWidget::NativeDestruct()
{
	// Attribute 구독 해제 — 보스 사망·위젯 파괴 시 댕글링 콜백 방지
	if (IsValid(BoundAttributes))
	{
		BoundAttributes->OnHealthChanged.RemoveDynamic(this, &UBWBossHealthBarWidget::OnHealthChangedCallback);
		BoundAttributes = nullptr;
	}

	Super::NativeDestruct();
}

void UBWBossHealthBarWidget::OnHealthChangedCallback(float NewValue, float MaxValue)
{
	const float Percent = (MaxValue > 0.f) ? FMath::Clamp(NewValue / MaxValue, 0.f, 1.f) : 0.f;

	if (HealthBar)
	{
		HealthBar->SetPercent(Percent);
	}
}
