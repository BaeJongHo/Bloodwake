// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BWAttributeComponent.generated.h"

// 현재값, 최대값을 전달 → HUD가 비율 계산
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBWOnAttributeChanged, float, NewValue, float, MaxValue);

// 스태미나 0 도달 알림 (질주 중단 트리거용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBWOnStaminaDepleted);

/**
 * 플레이어·적·보스 공용 자원(Health / Stamina / Focus) 관리 컴포넌트.
 * 소비·회복 API, regen delay + 반복 타이머 회복, 값 변경 델리게이트를 제공한다.
 * Focus는 자연 regen 없음(명시적 RestoreFocus API로만 회복).
 * 1단계: GAS 미적용, 싱글플레이 전용, 리플리케이션 코드 없음.
 */
UCLASS(Blueprintable, ClassGroup = (Bloodwake), meta = (BlueprintSpawnableComponent))
class BLOODWAKE_API UBWAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBWAttributeComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ── 소비 / 회복 API ──────────────────────────────────────────────

	/**
	 * 스태미나를 Amount만큼 소비한다.
	 * 충분한 스태미나가 있으면 차감 후 true 반환, 없으면 false.
	 * 0에 도달하면 OnStaminaDepleted 브로드캐스트.
	 */
	UFUNCTION(BlueprintCallable, Category = "Attributes|Stamina")
	bool ConsumeStamina(float Amount);

	/** 스태미나를 Amount만큼 직접 회복한다(최대값 클램프). */
	UFUNCTION(BlueprintCallable, Category = "Attributes|Stamina")
	void RestoreStamina(float Amount);

	/** 체력을 Amount만큼 직접 회복한다(최대값 클램프). */
	UFUNCTION(BlueprintCallable, Category = "Attributes|Health")
	void RestoreHealth(float Amount);

	/** 기력(Focus)을 Amount만큼 직접 회복한다(최대값 클램프). */
	UFUNCTION(BlueprintCallable, Category = "Attributes|Focus")
	void RestoreFocus(float Amount);

	/** 기력(Focus)을 Amount만큼 소비한다. 충분하면 차감 후 true, 없으면 false. */
	UFUNCTION(BlueprintCallable, Category = "Attributes|Focus")
	bool ConsumeFocus(float Amount);

	/** 피해를 적용한다. (1단계 골격 — 후속 데미지 시스템에서 확장) */
	UFUNCTION(BlueprintCallable, Category = "Attributes|Health")
	void ApplyDamage(float Amount);

	// ── 조회 헬퍼(BlueprintPure) ─────────────────────────────────────

	UFUNCTION(BlueprintPure, Category = "Attributes|Health")
	float GetHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Attributes|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Attributes|Stamina")
	float GetStamina() const { return CurrentStamina; }

	UFUNCTION(BlueprintPure, Category = "Attributes|Stamina")
	float GetMaxStamina() const { return MaxStamina; }

	UFUNCTION(BlueprintPure, Category = "Attributes|Focus")
	float GetFocus() const { return CurrentFocus; }

	UFUNCTION(BlueprintPure, Category = "Attributes|Focus")
	float GetMaxFocus() const { return MaxFocus; }

	/** 체력 비율(0~1). MaxHealth == 0이면 0 반환(0-division 가드). */
	UFUNCTION(BlueprintPure, Category = "Attributes|Health")
	float GetHealthPercent() const;

	/** 스태미나 비율(0~1). MaxStamina == 0이면 0 반환. */
	UFUNCTION(BlueprintPure, Category = "Attributes|Stamina")
	float GetStaminaPercent() const;

	/** 기력 비율(0~1). MaxFocus == 0이면 0 반환. */
	UFUNCTION(BlueprintPure, Category = "Attributes|Focus")
	float GetFocusPercent() const;

	/** 현재 스태미나가 Amount 이상인지 확인한다. */
	UFUNCTION(BlueprintPure, Category = "Attributes|Stamina")
	bool HasEnoughStamina(float Amount) const;

	/** 현재 스태미나가 Threshold 이상인지 확인한다(질주 자동 재개 판정용). */
	UFUNCTION(BlueprintPure, Category = "Attributes|Stamina")
	bool IsStaminaAboveThreshold(float Threshold) const;

	// ── 델리게이트 ───────────────────────────────────────────────────

	/** 체력이 변경될 때 브로드캐스트(NewValue, MaxValue). */
	UPROPERTY(BlueprintAssignable, Category = "Attributes|Health")
	FBWOnAttributeChanged OnHealthChanged;

	/** 스태미나가 변경될 때 브로드캐스트(NewValue, MaxValue). */
	UPROPERTY(BlueprintAssignable, Category = "Attributes|Stamina")
	FBWOnAttributeChanged OnStaminaChanged;

	/** 기력이 변경될 때 브로드캐스트(NewValue, MaxValue). */
	UPROPERTY(BlueprintAssignable, Category = "Attributes|Focus")
	FBWOnAttributeChanged OnFocusChanged;

	/** 스태미나가 0에 도달했을 때 브로드캐스트. */
	UPROPERTY(BlueprintAssignable, Category = "Attributes|Stamina")
	FBWOnStaminaDepleted OnStaminaDepleted;

	// ── 튜닝 데이터 (BP 자식에서 설정) ─────────────────────────────

	/** 최대 체력. BP 자식 DefaultsOnly에서 밸런싱. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes|Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	/** 최대 스태미나. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes|Stamina", meta = (ClampMin = "1.0"))
	float MaxStamina = 100.f;

	/** 최대 기력(Focus). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes|Focus", meta = (ClampMin = "1.0"))
	float MaxFocus = 100.f;

	/** 체력 초당 자연 회복량(0 = 회복 없음). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes|Health", meta = (ClampMin = "0.0"))
	float HealthRegenRate = 0.f;

	/** 스태미나 초당 자연 회복량. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes|Stamina", meta = (ClampMin = "0.0"))
	float StaminaRegenRate = 25.f;

	/** 마지막 소비 후 regen이 시작되기 전까지 대기 시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes|Regen", meta = (ClampMin = "0.0"))
	float RegenDelay = 0.8f;

	/** regen 타이머 호출 간격(초). 낮을수록 부드럽지만 비용 증가. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes|Regen", meta = (ClampMin = "0.01"))
	float RegenTickInterval = 0.1f;

	// ── 런타임 상태 (읽기 전용 조회용) ─────────────────────────────

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes|Health")
	float CurrentHealth = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes|Stamina")
	float CurrentStamina = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes|Focus")
	float CurrentFocus = 0.f;

private:
	/** regen 타이머 콜백. RegenDelay 경과 시 Health/Stamina 회복. 만충이면 타이머 정지. */
	void TickRegen();

	void StartRegenTimer();
	void StopRegenTimer();

	/** 마지막 스태미나 소비 이후 경과 시간을 반환한다. */
	float GetTimeSinceLastConsume() const;

	FTimerHandle RegenTimerHandle;

	/** GetWorld()->GetTimeSeconds() 기준 마지막 소비 기록. */
	float LastConsumeTime = 0.f;
};
