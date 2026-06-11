// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Equipment/BWEquipItem.h"
#include "Combat/BWCombatTypes.h"
#include "BWAnimNotify.generated.h"

class UBWCombatComponent;
class USkeletalMeshComponent;
class UAnimSequenceBase;

/**
 * BW 커스텀 AnimNotify 베이스 클래스.
 * 소유 캐릭터의 UBWCombatComponent를 조회하는 공통 헬퍼를 제공한다.
 * 향후 히트 윈도우 등 다른 BW 노티파이도 이 베이스를 상속한다.
 */
UCLASS(Abstract)
class BLOODWAKE_API UBWAnimNotify : public UAnimNotify
{
	GENERATED_BODY()

protected:
	/**
	 * MeshComp 소유 액터에서 UBWCombatComponent를 찾아 반환한다.
	 * 컴포넌트를 찾지 못하면 Warning 로그를 남기고 nullptr를 반환한다.
	 */
	UBWCombatComponent* GetCombatComponent(const USkeletalMeshComponent* MeshComp) const;
};

/**
 * 장비 슬롯을 지정 소켓으로 부착하는 AnimNotify.
 * TargetSlot(무기/방패)과 Destination(손/등)을 에디터에서 설정해
 * 한 노티파이 클래스로 뽑기/넣기 4가지 조합을 모두 커버한다.
 * 몽타주 트랙에 배치하여 애니메이터가 소켓 이동 타이밍을 직접 지정한다.
 */
UCLASS(meta = (DisplayName = "BW Attach Equip (Slot/Socket)"))
class BLOODWAKE_API UBWAnimNotify_AttachEquip : public UBWAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	                    const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 이동시킬 장비 슬롯(무기/방패). 노티파이를 배치한 몽타주가 어느 슬롯용인지 지정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Notify")
	EBWEquipSlot TargetSlot = EBWEquipSlot::Weapon;

	/** 이 노티파이 시점에 장비를 옮길 목적지 소켓(손=뽑기 완료, 등=넣기 완료). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Notify")
	EBWAttachDestination Destination = EBWAttachDestination::Hand;
};
