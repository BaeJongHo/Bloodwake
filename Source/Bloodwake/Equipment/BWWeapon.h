// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/BWEquipItem.h"
#include "Combat/BWCombatTypes.h"
#include "BWWeapon.generated.h"

class UDataTable;

/**
 * 무기 액터의 추상 베이스 클래스. ABWEquipItem에서 파생한다.
 * 무기 고유 데이터(데미지·포이즈 데미지·스태미나 소모·전투 타입·공격 DataTable·소켓명)를
 * BP 자식 설정 슬롯으로 노출한다.
 * 히트 판정은 UBWCombatComponent가 소유하는 Main/Second UBWWeaponCollisionComponent에 위임한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS(Abstract, Blueprintable)
class BLOODWAKE_API ABWWeapon : public ABWEquipItem
{
	GENERATED_BODY()

public:
	ABWWeapon();

	/** 무기 슬롯을 반환한다. CombatComponent의 슬롯 라우팅에 사용된다. */
	virtual EBWEquipSlot GetEquipSlot() const override { return EBWEquipSlot::Weapon; }

	// ── 전투 통계 접근자 ────────────────────────────────────────────────

	/** 기본 데미지를 반환한다. UBWWeaponCollisionComponent가 데미지 수치를 읽기 위해 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
	float GetBaseDamage() const { return BaseDamage; }

	/** 포이즈 데미지를 반환한다. 향후 포이즈 시스템 연동용. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
	float GetPoiseDamage() const { return PoiseDamage; }

	// ── 전투 타입·DataTable 접근자 ──────────────────────────────────────

	/** 이 무기의 전투 형태를 반환한다. CombatComponent가 현재 전투 타입 결정에 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Combat")
	EBWCombatType GetCombatType() const { return CombatType; }

	/**
	 * 이 무기의 공격/장착/해제 DataTable을 반환한다.
	 * CombatComponent가 공격 시 AttackComponent에 push하고, Equip/Unequip 행 조회에 사용한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Weapon|Combat")
	UDataTable* GetAttackDataTable() const { return AttackDataTable.Get(); }

	// ── Sweep 소켓 접근자 ────────────────────────────────────────────────

	/** Sweep 시작 소켓명을 반환한다. UBWWeaponCollisionComponent가 sweep 시 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Trace")
	FName GetTraceStartSocket() const { return TraceStartSocket; }

	/** Sweep 끝 소켓명을 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Trace")
	FName GetTraceEndSocket() const { return TraceEndSocket; }

	/** Sweep 캡슐 반경(cm)을 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Trace")
	float GetTraceRadius() const { return TraceRadius; }

	// ── 양손 소켓 오버라이드 접근자 ─────────────────────────────────────

	/**
	 * 양손 무기의 손(장착) 소켓 오버라이드를 반환한다.
	 * TwoHanded이면 TwoHandedEquipSocketName, 아니면 NAME_None(CombatComponent가 기본 소켓으로 폴백).
	 */
	UFUNCTION(BlueprintPure, Category = "Weapon|Sockets")
	FName GetHandSocketOverride() const;

	/**
	 * 양손 무기의 등(보관) 소켓 오버라이드를 반환한다.
	 * TwoHanded이면 TwoHandedUnequipSocketName, 아니면 NAME_None.
	 */
	UFUNCTION(BlueprintPure, Category = "Weapon|Sockets")
	FName GetHolsterSocketOverride() const;

protected:
	// ── 전투 통계 (BP 자식에서 밸런싱) ─────────────────────────────────

	/** 기본 데미지. BP 자식에서 밸런싱. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats", meta = (ClampMin = "0.0"))
	float BaseDamage = 10.f;

	/** 포이즈 데미지. BP 자식에서 밸런싱. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats", meta = (ClampMin = "0.0"))
	float PoiseDamage = 0.f;

	/** 공격 1회 스태미나 소모량. BP 자식에서 밸런싱. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats", meta = (ClampMin = "0.0"))
	float StaminaCostPerSwing = 0.f;

	// ── 전투 타입·DataTable (BP 자식에서 설정) ──────────────────────────

	/**
	 * 이 무기의 전투 형태. SwordAndShield 또는 TwoHanded.
	 * MeleeFists는 무기가 없을 때 CombatComponent가 자동 설정하므로 여기서는 사용하지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
	EBWCombatType CombatType = EBWCombatType::SwordAndShield;

	/**
	 * 이 무기의 공격/장착/해제 DataTable. RowStruct = FBWAttackComboRow.
	 * 행: Light/Running/Special/Heavy(공격 콤보) + Equip/Unequip(장착·해제 몽타주 1개).
	 * BP 자식에서 DT_<무기>Attacks 에셋을 지정한다(경로 하드코딩 금지).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
	TObjectPtr<UDataTable> AttackDataTable;

	// ── Sweep 소켓 (BP 자식에서 무기 SM 소켓과 일치하도록 지정) ─────────

	/** Sweep 시작 소켓명(칼날 밑동). 무기 SM에 추가된 소켓명과 일치시킨다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace")
	FName TraceStartSocket = TEXT("trace_start");

	/** Sweep 끝 소켓명(칼끝). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace")
	FName TraceEndSocket = TEXT("trace_end");

	/** Sweep 캡슐 반경(cm). 칼날 두께에 맞춰 BP에서 튜닝한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace", meta = (ClampMin = "0.0"))
	float TraceRadius = 8.f;

	// ── 양손 무기 소켓 오버라이드 (TwoHanded 전용, BP 자식에서 지정) ────

	/**
	 * 양손 무기 손(장착) 소켓명. CombatType==TwoHanded일 때만 사용된다.
	 * 캐릭터 스켈레톤에 추가한 소켓명과 일치시킨다(하드코딩 금지 — BP에서 지정).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sockets",
		meta = (EditCondition = "CombatType == EBWCombatType::TwoHanded"))
	FName TwoHandedEquipSocketName = TEXT("TwoHanded_Equip_Socket");

	/**
	 * 양손 무기 등(보관) 소켓명. CombatType==TwoHanded일 때만 사용된다.
	 * 캐릭터 스켈레톤에 추가한 소켓명과 일치시킨다(하드코딩 금지 — BP에서 지정).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sockets",
		meta = (EditCondition = "CombatType == EBWCombatType::TwoHanded"))
	FName TwoHandedUnequipSocketName = TEXT("TwoHanded_Unequip_Socket");
};
