// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWAttributeComponent.h"

#include "TimerManager.h"

UBWAttributeComponent::UBWAttributeComponent()
{
	// 자원 관리는 이벤트/타이머 기반. 매 프레임 틱 비활성.
	PrimaryComponentTick.bCanEverTick = false;
}

void UBWAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	// 현재값을 최대값으로 초기화
	CurrentHealth = MaxHealth;
	CurrentStamina = MaxStamina;
	CurrentFocus = MaxFocus;

	// 초기 상태를 HUD 등 구독자에게 브로드캐스트
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
	OnFocusChanged.Broadcast(CurrentFocus, MaxFocus);

	// 초기 regen 타이머 시작 (HealthRegenRate > 0이거나 StaminaRegenRate > 0이면 의미 있음)
	StartRegenTimer();
}

void UBWAttributeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopRegenTimer();

	Super::EndPlay(EndPlayReason);
}

// ── 소비 / 회복 API ───────────────────────────────────────────────────────────

bool UBWAttributeComponent::ConsumeStamina(float Amount)
{
	if (Amount <= 0.f)
	{
		return true;
	}

	if (CurrentStamina < Amount)
	{
		return false;
	}

	CurrentStamina = FMath::Max(0.f, CurrentStamina - Amount);

	// delay 기록 갱신
	if (UWorld* World = GetWorld())
	{
		LastConsumeTime = World->GetTimeSeconds();
	}

	OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);

	// 0에 도달하면 고갈 알림 (부동소수 오차 포함 — IsNearlyZero로 안전하게 판정)
	if (FMath::IsNearlyZero(CurrentStamina))
	{
		OnStaminaDepleted.Broadcast();
	}

	// 소비 발생 — regen 타이머가 멈춰있으면 재가동
	StartRegenTimer();

	return true;
}

void UBWAttributeComponent::RestoreStamina(float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}

	const float OldStamina = CurrentStamina;
	CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + Amount);

	if (!FMath::IsNearlyEqual(OldStamina, CurrentStamina))
	{
		OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
	}
}

void UBWAttributeComponent::RestoreHealth(float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);

	if (!FMath::IsNearlyEqual(OldHealth, CurrentHealth))
	{
		OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
	}
}

void UBWAttributeComponent::RestoreFocus(float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}

	const float OldFocus = CurrentFocus;
	CurrentFocus = FMath::Min(MaxFocus, CurrentFocus + Amount);

	if (!FMath::IsNearlyEqual(OldFocus, CurrentFocus))
	{
		OnFocusChanged.Broadcast(CurrentFocus, MaxFocus);
	}
}

bool UBWAttributeComponent::ConsumeFocus(float Amount)
{
	if (Amount <= 0.f)
	{
		return true;
	}

	if (CurrentFocus < Amount)
	{
		return false;
	}

	CurrentFocus = FMath::Max(0.f, CurrentFocus - Amount);
	OnFocusChanged.Broadcast(CurrentFocus, MaxFocus);
	return true;
}

void UBWAttributeComponent::ApplyDamage(float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}

	CurrentHealth = FMath::Max(0.f, CurrentHealth - Amount);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	// 사망 판정: 체력 0 도달 시 OnDeath를 1회만 발화한다.
	// 포이즈 계산은 후속 작업(TODO)으로 유지.
	if (FMath::IsNearlyZero(CurrentHealth) && !bDeathBroadcast)
	{
		bDeathBroadcast = true;
		OnDeath.Broadcast();
	}
}

// ── 조회 헬퍼 ────────────────────────────────────────────────────────────────

float UBWAttributeComponent::GetHealthPercent() const
{
	if (MaxHealth <= 0.f)
	{
		return 0.f;
	}
	return CurrentHealth / MaxHealth;
}

float UBWAttributeComponent::GetStaminaPercent() const
{
	if (MaxStamina <= 0.f)
	{
		return 0.f;
	}
	return CurrentStamina / MaxStamina;
}

float UBWAttributeComponent::GetFocusPercent() const
{
	if (MaxFocus <= 0.f)
	{
		return 0.f;
	}
	return CurrentFocus / MaxFocus;
}

bool UBWAttributeComponent::HasEnoughStamina(float Amount) const
{
	return CurrentStamina >= Amount;
}

bool UBWAttributeComponent::IsStaminaAboveThreshold(float Threshold) const
{
	return CurrentStamina >= Threshold;
}

// ── 내부: regen 타이머 ────────────────────────────────────────────────────────

void UBWAttributeComponent::TickRegen()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// regen delay 확인
	const bool bDelayPassed = GetTimeSinceLastConsume() >= RegenDelay;

	// Health regen (Focus는 자연 regen 없음)
	if (HealthRegenRate > 0.f && CurrentHealth < MaxHealth && bDelayPassed)
	{
		const float Gain = HealthRegenRate * RegenTickInterval;
		const float OldHealth = CurrentHealth;
		CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Gain);
		if (!FMath::IsNearlyEqual(OldHealth, CurrentHealth))
		{
			OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
		}
	}

	// Stamina regen
	if (StaminaRegenRate > 0.f && CurrentStamina < MaxStamina && bDelayPassed)
	{
		const float Gain = StaminaRegenRate * RegenTickInterval;
		const float OldStamina = CurrentStamina;
		CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + Gain);
		if (!FMath::IsNearlyEqual(OldStamina, CurrentStamina))
		{
			OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
		}
	}

	// 모든 regen 대상(Health/Stamina)이 만충이면 타이머 정지
	const bool bHealthFull = (HealthRegenRate <= 0.f) || (CurrentHealth >= MaxHealth);
	const bool bStaminaFull = (StaminaRegenRate <= 0.f) || (CurrentStamina >= MaxStamina);

	if (bHealthFull && bStaminaFull)
	{
		StopRegenTimer();
	}
}

void UBWAttributeComponent::StartRegenTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 이미 타이머가 실행 중이면 중복 등록하지 않음
	if (World->GetTimerManager().IsTimerActive(RegenTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		RegenTimerHandle,
		this,
		&UBWAttributeComponent::TickRegen,
		RegenTickInterval,
		/*bLoop=*/true
	);
}

void UBWAttributeComponent::StopRegenTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(RegenTimerHandle);
}

float UBWAttributeComponent::GetTimeSinceLastConsume() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return RegenDelay; // 월드 없으면 delay 경과로 취급
	}
	return World->GetTimeSeconds() - LastConsumeTime;
}
