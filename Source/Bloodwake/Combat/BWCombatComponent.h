// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Equipment/BWEquipItem.h"
#include "Combat/BWCombatTypes.h"
#include "BWCombatComponent.generated.h"

class ABWEquipItem;
class ABWPickUpItem;
class USkeletalMeshComponent;
class UBWStateComponent;
class UAnimMontage;
class ACharacter;

/**
 * 소유 캐릭터의 장비 인스턴스 보유 및 장착/해제/뽑기/넣기 로직을 담당하는 컴포넌트.
 * 소켓 부착의 단일 진입점으로, 플레이어·적·보스가 재사용할 수 있다.
 * 1단계: GAS 미적용, 싱글플레이 전용, 리플리케이션 없음.
 *
 * 무기 슬롯과 방패 슬롯을 독립 운용한다. 각 슬롯은 손/등 소켓을 별도로 보유한다.
 * 슬롯별 토글(ToggleWeapon / ToggleShield)은 장착/해제 몽타주를 재생하며,
 * 몽타주 중간의 UBWAnimNotify_AttachEquip 노티파이 시점에 실제 소켓 이동이 일어난다.
 */
UCLASS(Blueprintable, ClassGroup = (Bloodwake), meta = (BlueprintSpawnableComponent))
class BLOODWAKE_API UBWCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBWCombatComponent();

	virtual void BeginPlay() override;

	// ── 장비 API ────────────────────────────────────────────────────────

	/**
	 * 픽업이 준 장비 클래스를 슬롯에 분기 장착한다.
	 * 같은 슬롯에 기존 장비가 있으면 DropTransform 위치에 드롭 픽업을 생성하고 새 장비로 교체한다.
	 * 새 장비는 항상 등(보관)부터 시작한다.
	 * 성공 시 true. EquipItemClass가 null이거나 슬롯이 None이면 false.
	 * DropTransform: 교체 시 기존 장비를 드롭할 위치(주운 픽업의 Transform). 교체 없으면 미사용.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	bool EquipNewItem(TSubclassOf<ABWEquipItem> EquipItemClass, const FTransform& DropTransform);

	/**
	 * 무기 슬롯의 손↔등 상태를 토글한다.
	 * 현재 손에 들고 있으면 등으로, 등에 보관 중이면 손으로 전환.
	 * 장착/해제 몽타주를 재생하고, 노티파이 시점에 실제 소켓 이동이 일어난다.
	 * 몽타주 미설정 시 즉시 부착 + Warning 로그로 폴백한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	void ToggleWeapon();

	/**
	 * 방패 슬롯의 손↔등 상태를 토글한다.
	 * 현재 손에 들고 있으면 등으로, 등에 보관 중이면 손으로 전환.
	 * 장착/해제 몽타주를 재생하고, 노티파이 시점에 실제 소켓 이동이 일어난다.
	 * 몽타주 미설정 시 즉시 부착 + Warning 로그로 폴백한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	void ToggleShield();

	/** 무기가 현재 손에 뽑혀 있는지 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Combat|Equipment")
	bool IsWeaponDrawn() const { return bWeaponDrawn; }

	/** 방패가 현재 손에 들려 있는지 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Combat|Equipment")
	bool IsShieldDrawn() const { return bShieldDrawn; }

	/** 현재 보유 중인 무기 인스턴스를 반환한다. 없으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "Combat|Equipment")
	ABWEquipItem* GetEquippedWeapon() const { return EquippedWeapon; }

	/** 현재 보유 중인 방패 인스턴스를 반환한다. 없으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "Combat|Equipment")
	ABWEquipItem* GetEquippedShield() const { return EquippedShield; }

	/**
	 * 노티파이(UBWAnimNotify_AttachEquip)가 호출하는 부착 진입점.
	 * Destination(Hand/Back)에 따라 슬롯 장비를 해당 소켓으로 이동하고 bSlotDrawn을 갱신한다.
	 */
	void AttachSlotToSocket(EBWEquipSlot Slot, EBWAttachDestination Destination);

protected:
	// ── 소켓명 (캐릭터 스켈레톤 본 이름, BP 자식에서 실제 값 지정) ────

	/** 무기 손(장착) 소켓명. 캐릭터 스켈레톤에 추가한 소켓명과 일치해야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Sockets")
	FName WeaponHandSocketName = TEXT("hand_r_weapon");

	/** 무기 등(보관) 소켓명. 캐릭터 스켈레톤에 추가한 소켓명과 일치해야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Sockets")
	FName WeaponBackSocketName = TEXT("spine_weapon");

	/** 방패 손(장착) 소켓명. 캐릭터 스켈레톤에 추가한 소켓명과 일치해야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Sockets")
	FName ShieldHandSocketName = TEXT("hand_l_shield");

	/** 방패 등(보관) 소켓명. 캐릭터 스켈레톤에 추가한 소켓명과 일치해야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Sockets")
	FName ShieldBackSocketName = TEXT("spine_shield");

	// ── 장착/해제 몽타주 (BP 자식에서 슬롯별로 지정) ─────────────────

	/** 무기 뽑기(등→손) 몽타주. BP 자식에서 AM_EquipWeapon 에셋을 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Equip Montage")
	TObjectPtr<UAnimMontage> EquipWeaponMontage;

	/** 무기 넣기(손→등) 몽타주. BP 자식에서 AM_UnequipWeapon 에셋을 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Equip Montage")
	TObjectPtr<UAnimMontage> UnequipWeaponMontage;

	/** 방패 들기(등→손) 몽타주. BP 자식에서 AM_EquipShield 에셋을 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Equip Montage")
	TObjectPtr<UAnimMontage> EquipShieldMontage;

	/** 방패 넣기(손→등) 몽타주. BP 자식에서 AM_UnequipShield 에셋을 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Equip Montage")
	TObjectPtr<UAnimMontage> UnequipShieldMontage;

	// ── 드롭 설정 (BP 자식에서 설정) ────────────────────────────────

	/**
	 * 교체 드롭 시 스폰할 픽업 클래스. BP 자식에서 BP_PickUpItem을 지정한다.
	 * null이면 기존 장비만 Destroy하고 드롭 픽업을 생성하지 않는다(안전 폴백).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Drop")
	TSubclassOf<ABWPickUpItem> DropPickUpClass;

	/** 드롭 위치 보정(주운 픽업 Transform 기준 오프셋). BP 자식에서 튜닝한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Drop")
	FVector DropLocationOffset = FVector(0.f, 0.f, 0.f);

	/**
	 * 드롭 높이 보정(cm). 지면 안착 후 표식 메시가 바닥에 묻히지 않도록 위로 띄우는 값.
	 * 지면 트레이스 적중 지점(또는 트레이스 미적중 시 캐릭터 위치)에 더해진다. BP 자식에서 튜닝한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Drop", meta = (ClampMin = "0.0"))
	float DropHeightOffset = 0.f;

	/**
	 * 드롭 시 지면을 찾기 위한 하향 라인 트레이스 거리(cm).
	 * 캐릭터 위치(캡슐 중심)는 발보다 위에 있어 그대로 드롭하면 픽업이 공중에 뜬다.
	 * 캐릭터 위치에서 아래로 이만큼 ECC_Visibility 트레이스해 바닥에 안착시킨다(물리 시뮬레이션 불필요).
	 * 0이면 트레이스를 생략하고 캐릭터 위치에 그대로 드롭한다. BP 자식에서 튜닝한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Drop", meta = (ClampMin = "0.0"))
	float DropGroundTraceDistance = 500.f;

	/**
	 * BeginPlay에서 자동 장착할 시작 장비 클래스(선택).
	 * 없으면 빈손으로 시작한다. BP 자식에서 지정한다.
	 * 슬롯 분기로 자동 장착된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Equipment")
	TSubclassOf<ABWEquipItem> DefaultEquipItemClass;

	// ── 런타임 상태 (듀얼 슬롯) ────────────────────────────────────

	/** 현재 보유 중인 무기 인스턴스. GC 추적을 위해 TObjectPtr + UPROPERTY. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Equipment")
	TObjectPtr<ABWEquipItem> EquippedWeapon;

	/** 현재 보유 중인 방패 인스턴스. GC 추적을 위해 TObjectPtr + UPROPERTY. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Equipment")
	TObjectPtr<ABWEquipItem> EquippedShield;

	/** 무기가 현재 손에 뽑혀 있는지 여부. drawn 상태의 단일 진실 공급원(SSOT). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Equipment")
	bool bWeaponDrawn = false;

	/** 방패가 현재 손에 들려 있는지 여부. drawn 상태의 단일 진실 공급원(SSOT). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Equipment")
	bool bShieldDrawn = false;

private:
	/**
	 * 소유 캐릭터의 스켈레탈 메시 컴포넌트를 반환한다.
	 * BeginPlay에서 1회 캐시한다.
	 */
	USkeletalMeshComponent* GetOwnerMesh() const;

	/**
	 * 지정 소켓이 OwnerMesh에 존재하는지 확인한다.
	 * 없으면 Warning 로그를 남기고 false 반환 — 무음 부착 실패 방지.
	 */
	bool ValidateSocket(const FName& SocketName) const;

	/** 슬롯에 해당하는 손 소켓명을 반환한다. */
	FName GetHandSocketForSlot(EBWEquipSlot Slot) const;

	/** 슬롯에 해당하는 등 소켓명을 반환한다. */
	FName GetBackSocketForSlot(EBWEquipSlot Slot) const;

	/**
	 * 슬롯 런타임 아이템 포인터 참조를 반환한다.
	 * EquippedWeapon(Weapon 슬롯) 또는 EquippedShield(Shield 슬롯)의 참조를 반환한다.
	 * None 슬롯 전달 시 EquippedWeapon 참조를 반환하므로 호출 전 슬롯 유효성 확인 필수.
	 */
	TObjectPtr<ABWEquipItem>& GetSlotItemRef(EBWEquipSlot Slot);

	/**
	 * 슬롯 drawn 플래그 참조를 반환한다.
	 * bWeaponDrawn(Weapon) 또는 bShieldDrawn(Shield)의 참조를 반환한다.
	 * None 슬롯 전달 시 bWeaponDrawn 참조를 반환하므로 호출 전 슬롯 유효성 확인 필수.
	 */
	bool& GetSlotDrawnRef(EBWEquipSlot Slot);

	/**
	 * 기존 장비를 DropTransform 위치에 픽업으로 드롭한다.
	 * 클래스 캡처 → DropPickUpClass 스폰 → InitializeFromEquip 주입 → 기존 장비 Destroy 순서.
	 * DropPickUpClass가 null이면 기존 장비만 Destroy한다(안전 폴백).
	 */
	void DropEquipItem(ABWEquipItem* ItemToDrop, const FTransform& DropTransform);

	/**
	 * 슬롯·방향에 맞는 몽타주를 재생하고 Action.Equip/Unequip 태그를 건다.
	 * bDraw=true → Equip(뽑기, Action.Equip), false → Unequip(넣기, Action.Unequip).
	 * 몽타주 null/재생 실패 시 폴백으로 즉시 부착 + bool 갱신.
	 */
	void PlayEquipMontage(EBWEquipSlot Slot, bool bDraw);

	/** 슬롯·방향에 맞는 몽타주 에셋을 반환한다. 없으면 nullptr. */
	UAnimMontage* GetEquipMontage(EBWEquipSlot Slot, bool bDraw) const;

	/** 슬롯 장비를 손 소켓으로 부착하고 bSlotDrawn=true로 갱신한다. */
	void AttachSlotToHand(EBWEquipSlot Slot);

	/** 슬롯 장비를 등 소켓으로 부착하고 bSlotDrawn=false로 갱신한다. */
	void AttachSlotToBack(EBWEquipSlot Slot);

	/**
	 * 장착/해제 몽타주 종료 콜백. Action.Equip/Unequip 태그를 해제한다(입력 잠금 해제 안전망).
	 * 정상 종료·중단(bInterrupted) 모두 이 콜백이 호출된다.
	 */
	void OnEquipMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** 현재 장착/해제 모션이 진행 중인지 확인한다. 재입력 중복 차단에 사용. */
	bool IsEquipActionInProgress() const;

	/**
	 * 소유 캐릭터의 UBWStateComponent. BeginPlay에서 1회 캐시.
	 * 약참조이므로 사용 전 IsValid 확인 필수.
	 */
	TWeakObjectPtr<UBWStateComponent> CachedStateComponent;

	/** 소유 캐릭터 스켈레탈 메시 캐시. BeginPlay에서 1회 취득. */
	TWeakObjectPtr<USkeletalMeshComponent> CachedOwnerMesh;

	/**
	 * 소유 캐릭터 캐시(AnimInstance 접근용). BeginPlay에서 1회 캐시.
	 * 약참조이므로 사용 전 IsValid 확인 필수.
	 */
	TWeakObjectPtr<ACharacter> CachedOwnerCharacter;
};
