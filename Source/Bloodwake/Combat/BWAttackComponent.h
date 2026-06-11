// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Combat/BWAttackTypes.h"
#include "BWAttackComponent.generated.h"

class UDataTable;
class UAnimMontage;
class USkeletalMeshComponent;
class UBWStateComponent;
class UBWAttributeComponent;
class ACharacter;

/**
 * 플레이어·적·보스 공용 근접 공격(콤보) 컴포넌트.
 * 공격 종류 결정, 콤보 인덱스 관리, 입력 버퍼링, 스태미나 소모, 몽타주 재생, 상태 태그 운용을 담당한다.
 * 로직은 C++, 몽타주·스태미나 수치는 DataTable(DT_PlayerAttacks)로 외부화한다.
 * Tick 비활성. BeginPlay에서 소유 액터의 컴포넌트를 1회 캐시한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS(Blueprintable, ClassGroup = (Bloodwake), meta = (BlueprintSpawnableComponent))
class BLOODWAKE_API UBWAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBWAttackComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ── 입력 진입점 ──────────────────────────────────────────────────

	/**
	 * 캐릭터 입력 콜백(OnAttackStarted / OnAttackHold / OnHeavyAttack)이 호출하는 진입점.
	 * 입력 종류와 현재 상태(Sprint 태그 보유 여부)로 공격 종류를 결정하고 발동을 시도한다.
	 * 발동 흐름:
	 *   PrimaryHold                             → Special(단발)
	 *   PrimaryTap + Character.State.Sprint 태그 → Running(단발)
	 *   PrimaryTap                              → Light(콤보)
	 *   Heavy                                   → Heavy(콤보)
	 * 공격 중 아니면 콤보 첫 단계(0)를 시작한다.
	 * 콤보 윈도우 열림이면 다음 인덱스로 진행한다(Light/Heavy 콤보형만).
	 * 콤보 윈도우 닫힘이면 버퍼에 저장해 다음 OpenComboWindow에서 소비한다.
	 */
	void RequestAttack(EBWAttackInputKind InputKind);

	/**
	 * 현재 Attack 태그(Character.Attack.* 중 하나)를 보유 중인지 확인한다.
	 * 캐릭터 이동·점프·구르기 차단 게이트에 사용한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Attack")
	bool IsAttacking() const;

	// ── AnimNotifyState / AnimNotify 진입점 ──────────────────────────

	/**
	 * UBWComboAttackWindowNotifyState::NotifyBegin이 호출한다.
	 * 콤보 윈도우를 열고, 버퍼된 입력이 있으면 즉시 소비한다.
	 */
	void OpenComboWindow();

	/**
	 * UBWComboAttackWindowNotifyState::NotifyEnd가 호출한다.
	 * 콤보 윈도우를 닫고, 미소비 버퍼를 폐기한다.
	 */
	void CloseComboWindow();

	/**
	 * UBWComboAttackEndNotify::Notify가 호출한다.
	 * 콤보 인덱스를 0으로 초기화하고 Attack 태그를 해제한다.
	 */
	void NotifyComboEnd();

	// ── 노티파이 헬퍼 (정적) ─────────────────────────────────────────

	/**
	 * MeshComp 소유 액터에서 UBWAttackComponent를 찾아 반환한다.
	 * NotifyState / Notify 양쪽이 공통으로 호출하는 정적 헬퍼.
	 * 컴포넌트를 찾지 못하면 Warning 로그를 남기고 nullptr를 반환한다.
	 */
	static UBWAttackComponent* FindAttackComponent(const USkeletalMeshComponent* MeshComp);

protected:
	// ── BP/DataTable 설정용 데이터 ──────────────────────────────────

	/**
	 * 공격 몽타주·스태미나 테이블. BP 자식에서 DT_PlayerAttacks를 지정한다.
	 * RowStruct = FBWAttackComboRow, 행 키 = Light/Running/Special/Heavy.
	 * 경로 하드코딩 금지(CLAUDE.md 6.3).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Attack")
	TObjectPtr<UDataTable> AttackDataTable;

	/**
	 * Tap→Hold 전환 허용 시간(초).
	 * Started(Light 시작) 후 경과 시간이 이 값 미만이면 Hold 충족 시 Light를 중단하고 Special로 전환한다.
	 * IA 에셋의 HoldTimeThreshold(0.2~0.3s 권장)보다 작게 설정해야 전환이 자연스럽다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Attack", meta = (ClampMin = "0.0"))
	float HoldGraceTime = 0.15f;

	// ── 런타임 상태 (읽기 전용 노출) ────────────────────────────────

	/** 현재 진행 중인 공격 종류. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Attack")
	EBWAttackType CurrentAttackType = EBWAttackType::Light;

	/** 현재 콤보 단계(0-based). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Attack")
	int32 CurrentComboIndex = 0;

	/** 콤보 입력 윈도우 열림 여부. NotifyState Begin/End가 토글한다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Attack")
	bool bComboWindowOpen = false;

	// ── GC 추적이 필요한 Active Montage ─────────────────────────────

	/**
	 * 현재 재생 중인 공격 몽타주. End 델리게이트 식별에 사용한다.
	 * GC 추적이 필요하므로 UPROPERTY + TObjectPtr.
	 */
	UPROPERTY()
	TObjectPtr<UAnimMontage> ActiveMontage;

private:
	// ── private 구현부 ────────────────────────────────────────────────

	/**
	 * 지정 공격 종류의 StepIndex 단계를 시작한다.
	 * Row 조회 → Steps 경계 검사 → 스태미나 체크/소비 → 상태 태그 부착 → 몽타주 재생 + End 델리게이트 바인딩.
	 * 몽타주 재생 실패 시 태그 즉시 해제(상태 갇힘 방지).
	 */
	void StartAttack(EBWAttackType Type, int32 StepIndex);

	/**
	 * 몽타주 종료 안전망 콜백(Montage_SetEndDelegate 바인딩).
	 * ComboEnd 노티파이가 미배치·미도달인 경우에도 Attack 태그 해제 + 인덱스 0을 보장한다.
	 * Roll의 OnRollMontageEnded 안전망 패턴과 동일.
	 */
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** AttackDataTable에서 Type에 해당하는 Row를 찾아 반환한다. 없으면 nullptr. */
	const FBWAttackComboRow* FindRow(EBWAttackType Type) const;

	/**
	 * 공격 타입 → 상태 GameplayTag 매핑.
	 * Light→Character_Attack_Light 등. BWGameplayDefine.h:27-30 참조.
	 */
	FGameplayTag GetTagForAttackType(EBWAttackType Type) const;

	/** 공격 타입 → DataTable RowName 변환. enum 이름과 동일한 FName을 반환한다. */
	FName GetRowNameForAttackType(EBWAttackType Type) const;

	// ── 캐시 (TWeakObjectPtr, BeginPlay 1회 취득) ────────────────────

	/** 소유 캐릭터의 StateComponent. 상태 태그 추가/제거/질의에 사용. */
	TWeakObjectPtr<UBWStateComponent> CachedStateComponent;

	/** 소유 캐릭터의 AttributeComponent. 스태미나 체크/소비에 사용. */
	TWeakObjectPtr<UBWAttributeComponent> CachedAttributeComponent;

	/** 소유 캐릭터의 SkeletalMeshComponent. AnimInstance 접근에 사용. */
	TWeakObjectPtr<USkeletalMeshComponent> CachedOwnerMesh;

	/** 소유 ACharacter. AnimInstance 접근용 캐시. */
	TWeakObjectPtr<ACharacter> CachedOwnerCharacter;

	// ── 입력 버퍼 / 타이밍 (원시 타입, UPROPERTY 불필요) ─────────────

	/** 버퍼된 입력 종류. bHasBufferedInput이 true일 때만 유효. */
	EBWAttackInputKind BufferedInputKind = EBWAttackInputKind::PrimaryTap;

	/** 버퍼에 입력이 저장돼 있는지 여부. */
	bool bHasBufferedInput = false;

	/**
	 * 가장 최근 PrimaryTap 입력 발생 시각(GetWorld()->GetTimeSeconds()).
	 * HoldGraceTime과 비교해 Tap→Hold 전환(Light→Special 캔슬)을 판정한다.
	 */
	float LastPrimaryTapTime = 0.f;
};
