// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BWEquipItem.generated.h"

class UStaticMeshComponent;
class UStaticMesh;

/**
 * 장비가 장착될 슬롯 식별자.
 * 타입 분기(Cast)보다 가상 함수 + 열거형이 더 확장적이다 — 향후 양손무기/탈리스만 슬롯 추가 시 enum만 늘리면 된다.
 * CombatComponent의 슬롯 라우팅은 단일 switch로 처리한다.
 */
UENUM(BlueprintType)
enum class EBWEquipSlot : uint8
{
    None   UMETA(DisplayName = "None"),
    Weapon UMETA(DisplayName = "Weapon"),
    Shield UMETA(DisplayName = "Shield"),
};

/**
 * 장비(무기·방패) 액터의 최상위 추상 베이스 클래스.
 * 공통 스태틱 메시 컴포넌트, 캐릭터 소켓 부착 / 보관 / 장착 인터페이스를 제공한다.
 * 직접 인스턴스화하지 않는다 — ABWWeapon 또는 ABWShield(또는 그 BP 자식)로만 사용한다.
 *
 * 소켓명은 이 클래스가 보유하지 않는다. 소켓은 캐릭터/CombatComponent의 책임이므로
 * MoveToHand / MoveToHolster 가 소켓명을 인자로 받는다.
 */
UCLASS(Abstract, Blueprintable)
class BLOODWAKE_API ABWEquipItem : public AActor
{
	GENERATED_BODY()

public:
	ABWEquipItem();

	/**
	 * 지정 메시 소켓에 부착한다. SnapToTargetNotIncludingScale 규칙 사용.
	 * 파생 클래스가 재정의 가능(스켈레탈 메시 장비 등).
	 */
	virtual void AttachToCharacter(USceneComponent* AttachTarget, FName SocketName);

	/**
	 * 보관(등) 상태로 전환 — Holster 소켓에 부착.
	 * CombatComponent가 소켓명을 전달한다.
	 */
	void MoveToHolster(USceneComponent* AttachTarget, FName HolsterSocketName);

	/**
	 * 장착(손) 상태로 전환 — Hand 소켓에 부착.
	 * CombatComponent가 소켓명을 전달한다.
	 */
	void MoveToHand(USceneComponent* AttachTarget, FName HandSocketName);

	/**
	 * 부착 중 콜리전·물리를 비활성화한다.
	 * 장비를 스폰한 직후 호출해 캐릭터와의 충돌을 방지한다.
	 */
	void SetEquippedPhysicsState();

	/**
	 * 이 장비가 속하는 슬롯을 반환한다.
	 * 베이스는 None을 반환. ABWWeapon은 Weapon, ABWShield는 Shield를 반환한다.
	 * CombatComponent가 슬롯 라우팅에 사용한다.
	 */
	virtual EBWEquipSlot GetEquipSlot() const { return EBWEquipSlot::None; }

	/**
	 * MeshComponent의 StaticMesh를 반환한다.
	 * MeshComponent가 protected이므로 ABWPickUpItem 등 외부에서 외형 메시를 읽기 위한 접근자.
	 * CDO에서도 안전하게 호출 가능하다.
	 */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	UStaticMesh* GetDisplayStaticMesh() const;

protected:
	/** 장비 외형 메시. BP 자식의 디테일 패널에서 SM_ 에셋을 지정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** UI 표시명. BP 자식에서 설정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FText DisplayName;

	/**
	 * 장비 메시 쪽 부착 보정용 소켓명(선택).
	 * 기본 NAME_None — 필요한 경우 BP 자식에서 지정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FName AttachOffsetSocket;
};
