// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BWPickUpItem.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class ABWEquipItem;

/**
 * 바닥에 놓인 픽업 액터.
 * 어떤 장비 클래스를 줄지 TSubclassOf로 보유하며, 캐릭터의 구체 트레이스에 감지된다.
 * 트레이스 성공 후 CombatComponent가 EquipItemClass를 받아 스폰한다.
 * OnConstruction에서 EquipItemClass CDO의 메시를 DisplayMesh에 자동 적용한다(에디터 미리보기 + 런타임 일관성).
 * 1단계: 픽업 소비는 단순 Destroy. 오브젝트 풀은 후속 작업.
 */
UCLASS(Blueprintable)
class BLOODWAKE_API ABWPickUpItem : public AActor
{
	GENERATED_BODY()

public:
	ABWPickUpItem();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	/**
	 * 이 픽업이 줄 장비 클래스를 반환한다.
	 * CombatComponent가 SpawnActor 시 사용한다.
	 */
	UFUNCTION(BlueprintPure, Category = "PickUp")
	TSubclassOf<ABWEquipItem> GetEquipItemClass() const { return EquipItemClass; }

	/**
	 * 드롭 재생성 시 CombatComponent가 호출하는 주입 경로.
	 * EquipItemClass를 세팅하고 DisplayMesh 외형을 즉시 갱신한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "PickUp")
	void InitializeFromEquip(TSubclassOf<ABWEquipItem> InEquipItemClass);

	/**
	 * 픽업 소비 처리. 획득 성공 후 CombatComponent에서 호출한다.
	 * 1단계: 액터를 즉시 파괴한다. 오브젝트 풀 반환은 후속 작업.
	 */
	UFUNCTION(BlueprintCallable, Category = "PickUp")
	void Consume();

protected:
	/**
	 * 트레이스 감지용 구체 콜리전. 반경 및 채널 응답은 BP/레벨 인스턴스에서 설정한다.
	 * ECC_Visibility에 Block 응답해 SweepMultiByChannel에 걸린다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PickUp")
	TObjectPtr<USphereComponent> CollisionSphere;

	/**
	 * 스태틱 메시 외형 장비(무기·방패) 미리보기. EquipItemClass CDO의 StaticMesh가 자동 적용된다.
	 * 방어구처럼 스켈레탈 메시 외형인 경우에는 숨겨지고 DisplaySkeletalMesh가 대신 표시된다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PickUp")
	TObjectPtr<UStaticMeshComponent> DisplayMesh;

	/**
	 * 스켈레탈 메시 외형 장비(방어구) 미리보기. EquipItemClass CDO의 스켈레탈 메시가 자동 적용된다.
	 * 애님 인스턴스 없이 레퍼런스 포즈로만 표시되며(시각 표식 전용), StaticMesh 장비일 때는 숨겨진다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PickUp")
	TObjectPtr<USkeletalMeshComponent> DisplaySkeletalMesh;

	/**
	 * 이 픽업이 줄 장비 클래스. 레벨 배치 인스턴스마다 다른 장비를 지정할 수 있다.
	 * 예: BP_Weapon_Greatsword, BP_Shield_Tower 등 ABWEquipItem 파생 BP.
	 * 값이 변경되면 OnConstruction이 다시 호출되어 DisplayMesh가 즉시 갱신된다(에디터 미리보기).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PickUp")
	TSubclassOf<ABWEquipItem> EquipItemClass;

private:
	/**
	 * EquipItemClass의 CDO에서 외형 메시를 읽어 미리보기에 적용한다.
	 * CDO에 스켈레탈 메시(방어구)가 있으면 DisplaySkeletalMesh를, 없으면 DisplayMesh(StaticMesh)를 표시하고
	 * 다른 한쪽은 비워서 숨긴다. EquipItemClass/CDO/메시가 null이면 양쪽 모두 빈 상태로 폴백한다.
	 * OnConstruction / BeginPlay / InitializeFromEquip이 공통 호출한다.
	 */
	void ApplyDisplayMeshFromClass();

	/** 미리보기 메시를 모두 비우고 숨긴다(폴백 공통 처리). */
	void ClearDisplayMeshes();
};
