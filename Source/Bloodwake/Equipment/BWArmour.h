// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/BWEquipItem.h"
#include "BWArmour.generated.h"

class USkeletalMeshComponent;
class UBWAttributeComponent;

/**
 * 방어구 부위 식별자.
 * UBWCombatComponent의 TMap 키로 사용되며, 대응 신체 메시(Torso/Legs/Feet) 숨김 라우팅에도 쓰인다.
 * Gloves는 대응 기본 신체 메시가 없다 — SetBodyArmourHidden switch에서 default로 무시.
 */
UENUM(BlueprintType)
enum class EBWArmourType : uint8
{
    Chest   UMETA(DisplayName = "Chest"),   // 상의 → TorsoMesh
    Pants   UMETA(DisplayName = "Pants"),   // 하의 → LegsMesh
    Boots   UMETA(DisplayName = "Boots"),   // 신발 → FeetMesh
    Gloves  UMETA(DisplayName = "Gloves"),  // 장갑 (대응 신체 메시 없음)
};

/**
 * 방어구 액터의 베이스 클래스. ABWEquipItem에서 파생한다.
 *
 * 책임:
 *  - 방어구 1점의 외형(USkeletalMeshComponent ArmourMesh) 보유.
 *  - 부위(EBWArmourType)와 방어력(float DefenseAmount) 데이터 보유.
 *  - 플레이어 메시에 부착 + SetLeaderPoseComponent 설정(EquipItem).
 *  - 방어력 누적(IncreaseDefense) / 감산(DecreaseDefense) 트리거.
 *  - UBWCombatComponent가 TMap<EBWArmourType, TObjectPtr<ABWArmour>>로 관리한다.
 *
 * 설계 원칙:
 *  - 베이스 ABWEquipItem의 StaticMeshComponent(루트)는 그대로 유지 — 무기/방패 경로 무영향.
 *  - ArmourMesh는 루트에 attach된 별도 SkeletalMeshComponent. 외형 전용(콜리전 없음).
 *  - 로직은 C++, 데이터(SK_ 에셋·ArmourType·DefenseAmount)는 BP 자식에서 설정.
 *  - 직접 인스턴스화하지 않는다 — BP_Armour_* 자식 클래스로만 사용.
 *  - 1단계: GAS 미적용, 싱글플레이 전용, 리플리케이션 없음.
 */
UCLASS(Abstract, Blueprintable)
class BLOODWAKE_API ABWArmour : public ABWEquipItem
{
	GENERATED_BODY()

public:
	ABWArmour();

	// ── ABWEquipItem 오버라이드 ─────────────────────────────────────

	/** Armour 슬롯을 반환한다. CombatComponent 슬롯 라우팅에 사용. */
	virtual EBWEquipSlot GetEquipSlot() const override { return EBWEquipSlot::Armour; }

	/**
	 * 부착 대상 컴포넌트를 ArmourMesh(SkeletalMesh)로 오버라이드한다.
	 * 무기/방패의 StaticMesh 경로와 독립적으로 동작한다.
	 */
	virtual USceneComponent* GetAttachMeshComponent() const override;

	// ── 방어구 데이터 접근자 ────────────────────────────────────────

	/** 방어구 부위를 반환한다. CombatComponent의 TMap 키 및 신체 메시 라우팅에 사용. */
	UFUNCTION(BlueprintPure, Category = "Armour")
	EBWArmourType GetArmourType() const { return ArmourType; }

	/** 이 방어구의 방어력 기여치를 반환한다. AttributeComponent 누적에 사용. */
	UFUNCTION(BlueprintPure, Category = "Armour")
	float GetDefenseAmount() const { return DefenseAmount; }

	// ── 장착 / 해제 ────────────────────────────────────────────────

	/**
	 * 방어구를 Owner 플레이어 메시에 부착하고 LeaderPose를 설정한다.
	 *
	 * 처리 순서:
	 *  1. 이 액터의 루트를 OwnerMesh에 attach(SnapToTargetNotIncludingScale, NAME_None).
	 *  2. ArmourMesh->SetLeaderPoseComponent(OwnerMesh) — 플레이어 본 트랜스폼 추종.
	 *  3. Attribute->IncreaseDefense(DefenseAmount).
	 *
	 * @param OwnerMesh  부착 대상 플레이어 스켈레탈 메시(null 시 조용히 반환).
	 * @param Attribute  방어력을 누적할 AttributeComponent(null 허용 — 방어력 처리 건너뜀).
	 */
	UFUNCTION(BlueprintCallable, Category = "Armour")
	void EquipItem(USkeletalMeshComponent* OwnerMesh, UBWAttributeComponent* Attribute);

	/**
	 * 방어구 부착을 해제한다.
	 *  1. ArmourMesh->SetLeaderPoseComponent(nullptr) — 댕글링 본 참조 방지.
	 *  2. DetachFromActor(KeepWorldTransform) — 드롭/파괴 전 월드 공간 유지.
	 *  3. Attribute->DecreaseDefense(DefenseAmount).
	 *
	 * 드롭/파괴는 UBWCombatComponent가 담당한다.
	 *
	 * @param Attribute  방어력을 차감할 AttributeComponent(null 허용).
	 */
	UFUNCTION(BlueprintCallable, Category = "Armour")
	void UnequipItem(UBWAttributeComponent* Attribute);

protected:
	// ── 컴포넌트 ────────────────────────────────────────────────────

	/**
	 * 방어구 외형 스켈레탈 메시. 루트 MeshComponent(StaticMesh)에 attach된다.
	 * LeaderPose로 메인 플레이어 메시의 본 트랜스폼을 추종한다. 콜리전 없음(외형 전용).
	 * BP 자식의 (상속됨) ArmourMesh 컴포넌트에서 SK_ 에셋을 지정한다(새 컴포넌트 추가 금지).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Armour")
	TObjectPtr<USkeletalMeshComponent> ArmourMesh;

	// ── BP 설정용 데이터 ────────────────────────────────────────────

	/**
	 * 방어구 부위. UBWCombatComponent TMap 키로 사용.
	 * BP 자식 DefaultsOnly에서 Chest/Pants/Boots/Gloves 중 하나를 설정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armour")
	EBWArmourType ArmourType = EBWArmourType::Chest;

	/**
	 * 이 방어구가 제공하는 방어력. IncreaseDefense/DecreaseDefense 인자로 사용.
	 * BP 자식 DefaultsOnly에서 밸런싱한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armour", meta = (ClampMin = "0.0"))
	float DefenseAmount = 10.f;
};
