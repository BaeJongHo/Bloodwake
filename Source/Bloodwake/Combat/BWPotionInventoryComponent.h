// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BWPotionInventoryComponent.generated.h"

class ABWPotion;
class UBWAttributeComponent;
class UBWStateComponent;

// 포션 수량 변경 시 HUD 갱신용. 인자: 새 수량.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBWOnUpdatePotionAmount, int32, NewQuantity);

/**
 * 플레이어의 포션(에스트병류) 인벤토리를 관리하는 컴포넌트.
 * 수량 보유, 회복 실행(AttributeComponent에 위임), 포션 액터 스폰/디스폰,
 * 수량 변경 시 HUD 델리게이트 브로드캐스트를 담당한다.
 * 1단계: GAS 미적용, 싱글플레이 전용. Tick 비활성.
 */
UCLASS(Blueprintable, ClassGroup = (Bloodwake), meta = (BlueprintSpawnableComponent))
class BLOODWAKE_API UBWPotionInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBWPotionInventoryComponent();

	virtual void BeginPlay() override;

	// ── 핵심 API ────────────────────────────────────────────────────────

	/**
	 * 포션을 마신다. 수량 > 0이고 AttributeComponent가 유효하면
	 * RestoreHealth(PotionHealAmount) → 수량-- → BroadcastPotionUpdate() 순으로 실행한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Potion")
	void DrinkPotion();

	/**
	 * PotionClass로 포션 액터를 스폰해 소유 캐릭터 메시의 PotionSocketName 소켓에 부착한다.
	 * PotionActor가 이미 존재하면 먼저 DespawnPotion()을 호출해 정리한다(재진입 안전).
	 * Owner/PotionClass/소켓/World 유효성을 가드한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Potion")
	void SpawnPotion();

	/**
	 * 스폰된 PotionActor를 파괴하고 nullptr로 초기화한다.
	 * PotionActor가 없으면 무동작(안전 중복 호출 허용).
	 */
	UFUNCTION(BlueprintCallable, Category = "Potion")
	void DespawnPotion();

	/**
	 * 포션 수량을 직접 설정한다(0~MaxPotionQuantity 클램프 후 BroadcastPotionUpdate).
	 * 회복 지점이나 상점 등 외부에서 수량을 조정할 때 사용한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Potion")
	void SetPotionQuantity(int32 InQuantity);

	/** 현재 포션 수량을 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Potion")
	int32 GetPotionQuantity() const { return PotionQuantity; }

	/** OnUpdatePotionAmount 델리게이트에 현재 수량을 브로드캐스트한다. */
	UFUNCTION(BlueprintCallable, Category = "Potion")
	void BroadcastPotionUpdate();

	/**
	 * 포션을 마실 수 있는지 확인한다(PotionQuantity > 0).
	 * 입력 콜백의 조기 반환 게이트로 사용한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Potion")
	bool CanDrink() const;

	// ── 델리게이트 ───────────────────────────────────────────────────────

	/** 포션 수량이 변경될 때 브로드캐스트(NewQuantity). HUD가 구독한다. */
	UPROPERTY(BlueprintAssignable, Category = "Potion")
	FBWOnUpdatePotionAmount OnUpdatePotionAmount;

	// ── 튜닝 데이터 (BP 자식에서 설정) ──────────────────────────────────

	/** 1회 회복량. BP 자식(상속됨 PotionInventoryComponent)에서 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Potion", meta = (ClampMin = "0.0"))
	float PotionHealAmount = 50.f;

	/** 손 소켓 이름(예: hand_l_potion). BP 자식에서 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Potion")
	FName PotionSocketName = NAME_None;

	/** 스폰할 포션 액터 클래스. BP 자식에서 BP_Potion을 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Potion")
	TSubclassOf<ABWPotion> PotionClass;

	/** 최대 보유 수량. SetPotionQuantity 클램프 상한. BP 자식에서 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Potion", meta = (ClampMin = "0"))
	int32 MaxPotionQuantity = 5;

	/** 현재 수량(초기값). EditAnywhere로 인스턴스별 초기 수량을 디자이너가 지정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Potion", meta = (ClampMin = "0"))
	int32 PotionQuantity = 3;

	/** 스폰된 포션 액터 참조(런타임 GC 추적). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Potion")
	TObjectPtr<ABWPotion> PotionActor;

private:
	/** BeginPlay에서 1회 캐시한 AttributeComponent 참조. DrinkPotion에서 반복 탐색 방지(CLAUDE.md 4.1). */
	UPROPERTY(Transient)
	TWeakObjectPtr<UBWAttributeComponent> CachedAttributeComponent;

	/**
	 * BeginPlay에서 1회 캐시한 StateComponent 참조.
	 * DrinkPotion() 진입 시 DrinkingPotion 태그 보유 여부를 확인해 뒤늦은 노티파이를 차단한다.
	 * BWAttackComponent/BWCombatComponent의 CachedStateComponent 캐시 패턴과 동일.
	 */
	TWeakObjectPtr<UBWStateComponent> CachedStateComponent;
};
