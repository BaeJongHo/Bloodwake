// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BWPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UInputComponent;
class UBWAttributeComponent;
class UBWStateComponent;
class UBWCombatComponent;
class UBWAttackComponent;
class UAnimMontage;
struct FInputActionValue;

/**
 * 플레이어가 조작하는 3인칭 소울라이크 전투 캐릭터의 베이스 클래스.
 * 락온 기반 근접 전투를 위한 카메라 붐 + 팔로우 카메라 구성을 제공한다.
 * AttributeComponent(Health/Stamina/Focus), StateComponent(GameplayTagContainer), Sprint/Roll 입력 처리가 포함된다.
 * 스태미나 기반 Sprint: Hold IA 충족(Triggered) 시 SprintSpeed로 전환, 스태미나 소모, 0 고갈 시 중단 후 임계치 회복 시 자동 재개.
 * Roll: 별도 Tap IA(RollAction) 충족 시 Roll 상태 태그 부착 + RollMontage 재생. 몽타주의 AnimNotify가 EndRoll로 상태 해제. i-frame/스태미나 소모는 후속 작업.
 */
UCLASS()
class BLOODWAKE_API ABWPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABWPlayerCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** AttributeComponent 접근자. GameMode 등 외부에서 FindComponentByClass 우회 없이 접근하기 위해 제공. */
	UFUNCTION(BlueprintPure, Category = "Attributes")
	UBWAttributeComponent* GetAttributeComponent() const { return AttributeComponent; }

	/**
	 * 회피(Roll) 종료 처리. 회피 몽타주에 배치한 AnimNotify(AnimInstance의 AnimNotify_RollEnd)가 호출한다.
	 * Roll 상태 태그를 해제하며, StateComponent가 행동 상태 부재를 감지해 Normal로 자동 복귀시킨다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Roll")
	void EndRoll();

	/**
	 * 현재 공격 중인지 확인한다. AttackComponent->IsAttacking()에 위임한다.
	 * Move/StartJump/Roll 차단 게이트 및 BP에서 사용한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Attack")
	bool IsAttacking() const;

protected:
	/** MoveAction 콜백: 컨트롤러 Yaw 기준으로 입력 벡터를 월드 이동 방향으로 변환해 이동한다. 구르기/공격 중에는 무시한다. */
	void Move(const FInputActionValue& Value);

	// ── 공격 입력 콜백 ──────────────────────────────────────────────

	/**
	 * AttackAction(Started) 콜백 — IA_Attack 누르는 순간.
	 * AttackComponent->RequestAttack(PrimaryTap)에 위임한다.
	 * Sprint 태그 보유 중이면 Running, 아니면 Light로 분기(컴포넌트 내부 처리).
	 */
	void OnAttackStarted(const FInputActionValue& Value);

	/**
	 * AttackAction(Triggered) 콜백 — IA_Attack Hold 임계 충족 시.
	 * AttackComponent->RequestAttack(PrimaryHold)에 위임한다.
	 * 컴포넌트 내부에서 Light 극초기이면 Special로 전환, 아니면 무시한다.
	 */
	void OnAttackHold(const FInputActionValue& Value);

	/**
	 * HeavyAttackAction(Started) 콜백 — IA_HeavyAttack 누르는 순간.
	 * AttackComponent->RequestAttack(Heavy)에 위임한다.
	 */
	void OnHeavyAttack(const FInputActionValue& Value);

	/**
	 * InteractAction(Started) 콜백.
	 * 캐릭터 전방으로 구체 스윕을 1회 수행해 가장 가까운 ABWPickUpItem을 감지하고,
	 * CombatComponent에 장착을 위임한다. 성공 시 픽업 액터를 소비(Destroy)한다.
	 */
	void Interact(const FInputActionValue& Value);

	/**
	 * ToggleWeaponAction(Started) 콜백.
	 * CombatComponent->ToggleWeapon()에 위임한다.
	 */
	void ToggleWeapon(const FInputActionValue& Value);

	/**
	 * ToggleShieldAction(Started) 콜백.
	 * CombatComponent->ToggleShield()에 위임한다.
	 */
	void ToggleShield(const FInputActionValue& Value);

	/** LookAction 콜백: 마우스/우스틱 입력을 컨트롤러 회전(Yaw/Pitch)에 적용한다. */
	void Look(const FInputActionValue& Value);

	/** JumpAction(Started) 콜백: 구르기 중이 아니면 ACharacter::Jump를 호출한다. */
	void StartJump();

	/**
	 * 현재 구르기(Character.State.Roll) 상태인지 확인한다. 이동·점프 차단 판정에 사용.
	 * BP에서도 호출 가능(예: 공격 시작 전 "구르기 중이면 공격 금지" 게이트).
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Roll")
	bool IsRolling() const;

	// ── Sprint / Roll 입력 콜백 ─────────────────────────────────────

	/**
	 * SprintAction(Hold 전용 IA) Triggered 바인딩.
	 * Hold 임계 시간 충족 시 1회 호출된다. 조건 충족 시 질주를 시작한다.
	 * 주의: 기존 Started(누르자마자) 시작에서 Triggered(Hold 충족) 시작으로 변경됨.
	 *       Hold Time Threshold는 에디터 IA 에셋에서 튜닝한다(예: 0.15~0.2s).
	 */
	void StartSprint(const FInputActionValue& Value);

	/** SprintAction Completed/Canceled 바인딩. 질주를 종료한다. */
	void StopSprint(const FInputActionValue& Value);

	/**
	 * RollAction(Tap 전용 IA) Triggered 바인딩.
	 * 짧은 탭 입력 충족 시 Roll 상태 태그를 부착하고 RollMontage를 재생한다.
	 * 상태 해제는 몽타주에 배치한 AnimNotify(→ EndRoll)가 담당한다.
	 * TODO(후속): 스태미나 소모 체크, i-frame 활성화.
	 */
	void Roll(const FInputActionValue& Value);

	/**
	 * RollMontage 종료 콜백(Montage_SetEndDelegate 바인딩).
	 * 정상 종료든 다른 몽타주(공격 등)에 의한 중단(bInterrupted=true)이든 호출되어,
	 * RollEnd 노티파이가 불리지 못한 경우에도 Roll 상태를 보장 해제한다(입력 먹통 방지).
	 */
	void OnRollMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// ── Sprint 상태 전이 ────────────────────────────────────────────

	/** MaxWalkSpeed를 SprintSpeed로 전환하고 스태미나 소모 타이머를 시작한다. */
	void BeginSprinting();

	/** MaxWalkSpeed를 WalkSpeed로 복귀하고 스태미나 소모 타이머를 정지한다. */
	void EndSprinting();

	/** SprintDrainTimerHandle 콜백: 매 Interval마다 스태미나를 소모한다. 실패 시 질주 중단. */
	void TickSprintDrain();

	// ── AttributeComponent 델리게이트 콜백 (UFUNCTION 필수) ─────────

	/** OnStaminaDepleted 구독: 질주 중이면 강제 종료, 버튼 유지 상태는 보존(자동 재개 대기). */
	UFUNCTION()
	void HandleStaminaDepleted();

	/** OnStaminaChanged 구독: 자동 재개 판정(bSprintInputHeld && !bIsSprinting && 임계치 이상). */
	UFUNCTION()
	void HandleStaminaChanged(float NewValue, float MaxValue);

	/** 질주를 시작할 수 있는 조건을 확인한다. */
	bool CanSprint() const;

protected:
	// ── 카메라 ──────────────────────────────────────────────────────

	/** 캐릭터와 카메라 사이 거리를 두고 충돌을 보정하는 카메라 붐. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** 카메라 붐 끝에 부착되는 3인칭 팔로우 카메라. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// ── 입력 에셋 (BP 자식에서 에셋 지정) ──────────────────────────

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	/**
	 * Sprint(Hold) 전용 입력 액션. BP 자식에서 IA_Sprint 에셋을 지정한다.
	 * 에셋에 UInputTriggerHold를 부착해 Hold 충족 시 Triggered가 발생하도록 설정한다(에디터 작업).
	 * 같은 물리 키에 RollAction(Tap IA)도 함께 IMC에 매핑한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	/**
	 * Roll(Tap) 전용 입력 액션. BP 자식에서 IA_Roll 에셋을 지정한다(신규 에셋, 에디터 후속 작업).
	 * 에셋에 UInputTriggerTap을 부착해 짧은 탭 충족 시 Triggered가 발생하도록 설정한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> RollAction;

	/**
	 * Interact 입력 액션. BP 자식에서 IA_Interact 에셋을 지정한다.
	 * 누르는 순간 1회(Started) 발동 — 구체 스윕으로 근처 픽업을 감지해 장착을 시도한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	/**
	 * 무기 손↔등 토글 입력 액션. BP 자식에서 IA_ToggleWeapon 에셋을 지정한다.
	 * 누르는 순간 1회(Started) 발동 — CombatComponent->ToggleWeapon()에 위임한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ToggleWeaponAction;

	/**
	 * 방패 손↔등 토글 입력 액션. BP 자식에서 IA_ToggleShield 에셋을 지정한다.
	 * 누르는 순간 1회(Started) 발동 — CombatComponent->ToggleShield()에 위임한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ToggleShieldAction;

	/**
	 * Attack(약공격·대쉬·특수) 입력 액션. BP 자식에서 IA_Attack 에셋을 지정한다.
	 * IA 에셋에 Hold 트리거를 붙여 Started(Tap)와 Triggered(Hold) 두 이벤트로 분기한다(에디터 작업).
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

	/**
	 * Heavy 공격 입력 액션. BP 자식에서 IA_HeavyAttack 에셋을 지정한다.
	 * 누르는 순간 1회(Started) 발동.
	 */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> HeavyAttackAction;

	// ── 컴포넌트 ────────────────────────────────────────────────────

	/** Health / Stamina / Focus 자원 관리 컴포넌트. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes")
	TObjectPtr<UBWAttributeComponent> AttributeComponent;

	/** 현재 행동 상태(Sprint / Roll / Attack 등)를 GameplayTagContainer로 관리하는 컴포넌트. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	TObjectPtr<UBWStateComponent> StateComponent;

	/** 장비 보유·장착·해제 로직을 담당하는 전투 컴포넌트. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBWCombatComponent> CombatComponent;

	/** 공격(콤보) 로직을 담당하는 컴포넌트. DataTable에서 몽타주·스태미나를 설정한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBWAttackComponent> AttackComponent;

	// ── 인터랙션 튜닝 데이터 (BP 자식에서 설정) ─────────────────────

	/** 구체 트레이스 반경(cm). BP 자식에서 튜닝한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.0"))
	float InteractTraceRadius = 100.f;

	/** 캐릭터 전방 스윕 거리(cm). BP 자식에서 튜닝한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.0"))
	float InteractTraceDistance = 150.f;

	/** 구체 트레이스 채널. 커스텀 채널이 필요하면 BP에서 교체한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> InteractTraceChannel = ECC_Visibility;

	/** true이면 Interact 트레이스 결과를 뷰포트에 구체로 표시한다(디버그용). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bDrawInteractDebug = false;

	// ── Roll(회피) 데이터 (BP 자식에서 설정) ────────────────────────

	/**
	 * 회피(Roll) 시 재생할 몽타주. BP 자식에서 AM_ 회피 몽타주를 지정한다.
	 * 몽타주 종료 지점에 커스텀 AnimNotify("RollEnd")를 배치하면
	 * UBWPlayerAnimInstance::AnimNotify_RollEnd가 호출되어 EndRoll로 상태가 해제된다(에디터 작업).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Roll")
	TObjectPtr<UAnimMontage> RollMontage;

	/** 구르기 1회에 소비하는 스태미나. 부족하면 구르기가 발동하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Roll", meta = (ClampMin = "0.0"))
	float RollStaminaCost = 20.f;

	// ── Sprint 튜닝 데이터 (BP 자식에서 설정) ───────────────────────

	/** 보행 속도(cm/s). 질주 해제 시 복귀값. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Sprint", meta = (ClampMin = "0.0"))
	float WalkSpeed = 400.f;

	/** 질주 속도(cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Sprint", meta = (ClampMin = "0.0"))
	float SprintSpeed = 800.f;

	/** 초당 스태미나 소모량(질주 중). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Sprint", meta = (ClampMin = "0.0"))
	float SprintStaminaCostPerSecond = 15.f;

	/** 스태미나 소모 타이머 호출 간격(초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Sprint", meta = (ClampMin = "0.01"))
	float SprintStaminaDrainInterval = 0.1f;

	/**
	 * 스태미나 0 고갈 후 자동 재개 기준값.
	 * 현재 스태미나가 이 값 이상으로 회복되면 버튼이 눌린 상태일 때 자동으로 질주를 재개한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Sprint", meta = (ClampMin = "0.0"))
	float SprintResumeThreshold = 15.f;

	// ── Sprint 런타임 상태 ──────────────────────────────────────────

	/** 현재 질주 중인지 여부(에디터 디버그용). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement|Sprint")
	bool bIsSprinting = false;

private:
	/** Sprint 버튼이 눌린 채로 유지되고 있는지 여부. 자동 재개 판정에 사용. */
	bool bSprintInputHeld = false;

	/** 스태미나 소모 반복 타이머 핸들. */
	FTimerHandle SprintDrainTimerHandle;
};
