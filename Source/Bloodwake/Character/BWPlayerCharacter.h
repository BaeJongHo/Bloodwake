// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Combat/BWCombatInterface.h"
#include "Combat/BWCombatTypes.h"
#include "BWPlayerCharacter.generated.h"

enum class EBWArmourType : uint8;

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UInputComponent;
class UBWAttributeComponent;
class UBWStateComponent;
class UBWCombatComponent;
class UBWAttackComponent;
class UBWTargetingComponent;
class UBWPotionInventoryComponent;
class USkeletalMeshComponent;
class UAnimMontage;
class UParticleSystem;
class USoundBase;
struct FInputActionValue;

/**
 * 플레이어가 조작하는 3인칭 소울라이크 전투 캐릭터의 베이스 클래스.
 * 락온 기반 근접 전투를 위한 카메라 붐 + 팔로우 카메라 구성을 제공한다.
 * AttributeComponent(Health/Stamina/Focus), StateComponent(GameplayTagContainer), Sprint/Roll 입력 처리가 포함된다.
 * IBWCombatInterface 구현으로 공격 인터페이스 계약을 충족한다.
 * 피격 시스템: TakeDamage 오버라이드 + 방향별 히트 리액션 + VFX/사운드 + 사망 처리(Enemy 패턴 이식).
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS()
class BLOODWAKE_API ABWPlayerCharacter : public ACharacter, public IBWCombatInterface
{
	GENERATED_BODY()

public:
	ABWPlayerCharacter();

	/**
	 * 데미지 수신 오버라이드.
	 * FPointDamageEvent에서 ImpactPoint/ShotDirection을 추출해
	 * AttributeComponent에 위임 + 히트 리액션·VFX·사운드를 재생한다.
	 * Dead/Hit 상태에서는 무시한다(bIsDead 가드).
	 */
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	// ── IBWCombatInterface 구현 ─────────────────────────────────────────────

	/**
	 * 인터페이스 계약 충족용 얇은 구현.
	 * AttackComponent->RequestAttack(PrimaryTap)에 위임한다.
	 * 플레이어는 BT가 직접 호출하지 않으므로 OnEnded는 미사용.
	 */
	virtual void PerformAttack(EBWAttackType AttackType, FOnMontageEnded OnEnded) override;

	/**
	 * 현재 무적(i-frame) 상태인지 반환한다. IBWCombatInterface 구현.
	 * bIsInvincible이 true이면 TakeDamage가 Super::TakeDamage 이전에 조기 반환한다.
	 */
	virtual bool IsInvincible() const override { return bIsInvincible; }

	/**
	 * 무적 상태를 설정한다. IBWCombatInterface 구현.
	 * UBWAnimNotifyState_Invincibility가 구르기 몽타주 윈도우 진입/이탈 시 호출한다.
	 * Verbose 로그 포함 — 쉬핑 빌드에서 비용 0(CLAUDE.md 4.3).
	 */
	virtual void SetInvincible(bool bInInvincible) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** AttributeComponent 접근자. GameMode 등 외부에서 FindComponentByClass 우회 없이 접근하기 위해 제공. */
	UFUNCTION(BlueprintPure, Category = "Attributes")
	UBWAttributeComponent* GetAttributeComponent() const { return AttributeComponent; }

	/** PotionInventoryComponent 접근자. GameMode에서 HUD 델리게이트 배선에 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	UBWPotionInventoryComponent* GetPotionInventoryComponent() const { return PotionInventoryComponent; }

	/** CombatComponent 접근자. GameMode가 HUD 장비 위젯 델리게이트 배선에 사용한다. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	UBWCombatComponent* GetCombatComponent() const { return CombatComponent; }

	/**
	 * 포션 마시기 진행 중 피격 시 중단 처리.
	 * Character.State.DrinkingPotion 태그 해제 + DrinkMontage 정지 + PotionActor 디스폰.
	 * 태그가 없을 때 호출해도 무해(안전 중복 호출 허용).
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Potion")
	void InterruptWhileDrinkingPotion();

	/**
	 * 방어구 부위에 대응하는 기본 신체 메시의 가시성을 토글한다.
	 * UBWCombatComponent가 방어구 장착/해제 시 호출한다.
	 *  - Chest   → TorsoMesh
	 *  - Pants   → LegsMesh
	 *  - Boots   → FeetMesh
	 *  - Gloves  → 대응 메시 없음, 무시(크래시 방지)
	 * bHidden=true: 메시를 숨긴다(방어구가 그 부위를 가림).
	 * bHidden=false: 메시를 표시한다(방어구 해제 후 기본 신체 복원).
	 */
	UFUNCTION(BlueprintCallable, Category = "Body")
	void SetBodyArmourHidden(EBWArmourType Type, bool bHideBodyPart);

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

	/**
	 * 블로킹(가드)을 시작할 수 있는 조건을 확인한다.
	 * 사망·질주·공격·피격·구르기 중이 아니고 방패가 손에 들려 있어야 한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Block")
	bool CanBlock() const;

	/** 현재 블로킹(Character.State.Blocking 태그 보유) 상태인지 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Combat|Block")
	bool IsBlocking() const;

	/** 현재 블로킹 히트(Character.Action.BlockingHit 태그 보유, 가드 피격 경직) 상태인지 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Combat|Block")
	bool IsBlockingHit() const;

	/**
	 * 블로킹 히트(가드 피격 경직) 종료 처리. BlockingHit 몽타주에 배치한 AnimNotify
	 * (AnimInstance의 AnimNotify_BlockingHitEnd)가 회복 프레임에서 호출한다.
	 * Character.Action.BlockingHit 태그를 해제하고 경직 동안 잠갔던 입력을 복원한다.
	 * 노티파이 누락/몽타주 중단 시에는 몽타주 종료 콜백(OnBlockingHitMontageEnded)이 안전망으로 호출한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Block")
	void EndBlockingHit();

	/** 현재 패링 윈도우(Character.State.Parrying 태그 보유) 상태인지 반환한다. */
	UFUNCTION(BlueprintPure, Category = "Combat|Parry")
	bool IsParrying() const;

	/**
	 * 패링을 발동할 수 있는 조건을 종합 판정한다.
	 * 방패 없음/공격중/구르기중/피격중/블로킹중/블로킹히트중/패링중/사망/스태미나부족 중 하나라도 해당하면 false.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Parry")
	bool CanParry() const;

	/**
	 * 패링 성공 판정. TakeDamage에서 블로킹 분기 앞에 호출한다.
	 * 조건: IsParrying() && 공격자가 정면(ParryFrontDotThreshold 기준).
	 * @param Attacker 공격한 액터.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Parry")
	bool ParriedAttackSucceed(const AActor* Attacker) const;

	/**
	 * 패링 종료 처리. 패링 몽타주에 배치한 AnimNotify(AnimInstance의 AnimNotify_ParryEnd)가 호출한다.
	 * Character.State.Parrying 태그를 해제해 StateComponent를 Normal로 복귀시키며,
	 * Move()의 IsParrying() 게이트로 잠겨 있던 이동 입력을 다시 푼다("State 초기화 및 입력 키기").
	 * 몽타주 끝까지 기다리지 않고 원하는 회복 프레임에서 입력을 돌려주기 위한 진입점이다.
	 * 노티파이 누락/몽타주 중단 시에는 OnParryMontageEnded 안전망이 동일하게 태그를 해제한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Parry")
	void EndParry();

	/**
	 * 넉다운(쓰러짐) 상태인지 반환한다. Character.State.KnockdownHit 태그 보유 여부로 판단한다.
	 * 이 상태에서는 모든 입력이 차단되며, 넉다운 몽타주가 종료되면 EndKnockdown이 해제한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Knockdown")
	bool IsKnockedDown() const;

	/**
	 * 쓰러짐 구간을 끝내고 기상(GetUpMontage) 구간으로 전환한다.
	 * 이 시점에는 아직 KnockdownHit 태그와 입력 잠금이 유지된다 — 해제는 EndGetUp이 담당한다.
	 * 넉다운 몽타주에 배치한 AnimNotify가 호출하거나, 몽타주 종료 안전망
	 * (OnKnockdownMontageEnded)이 자동으로 호출한다.
	 * 기상 몽타주가 없거나 재생에 실패하면 즉시 EndGetUp()으로 넘어간다(입력 먹통 방지).
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Knockdown")
	void EndKnockdown();

	/**
	 * 기상 완료 — 넉다운 사이클의 최종 해제 지점. KnockdownHit 태그 해제 + 입력 복원.
	 * 기상 몽타주에 배치한 AnimNotify가 원하는 회복 프레임에서 호출하거나,
	 * 몽타주 종료 안전망(OnGetUpMontageEnded)이 자동으로 호출한다(EndParry와 동일 패턴).
	 * IsKnockedDown() 가드로 중복 복원을 차단해 입력 잠금/복원 1쌍을 보장한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Knockdown")
	void EndGetUp();

	/**
	 * 사망 상태인지 반환한다. AI(BWEnemyAIController) 등 외부에서 죽은 플레이어를
	 * 추격/공격 대상에서 제외하는 판정에 사용한다. BP에서도 조회 가능.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsDead() const { return bIsDead; }

	/**
	 * 현재 락온 중인지 반환한다. AnimInstance / BP에서 조회한다.
	 * TargetingComponent->IsLockedOn()에 위임한다.
	 * Thread-safe: 캐시된 bool(bIsLockedOnCached)을 반환해 워커 스레드(AnimInstance) 안전.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|LockOn")
	bool IsLockedOn() const;

	/**
	 * TargetingComponent에서 락온 상태 변경 시 호출한다.
	 * bIsLockedOnCached를 갱신해 AnimInstance가 게임스레드 bool만 읽도록 한다(레이스 컨디션 방지).
	 * 외부에서 직접 호출하지 않는다 — TargetingComponent 전용.
	 */
	void SetIsLockedOnCached(bool bInIsLockedOn)
	{
		bIsLockedOnCached = bInIsLockedOn;
	}

protected:
	/** MoveAction 콜백: 컨트롤러 Yaw 기준으로 입력 벡터를 월드 이동 방향으로 변환해 이동한다. 구르기/공격/피격/사망 중에는 무시한다. */
	void Move(const FInputActionValue& Value);

	/** LockOnAction(Started) 콜백 — IA_LockOn 누르는 순간. TargetingComponent->ToggleLockOn()에 위임한다. */
	void OnLockOn(const FInputActionValue& Value);

	/**
	 * SwitchTargetAction(Triggered) 콜백 — IA_SwitchTarget 입력.
	 * 입력 X축 값을 추출해 TargetingComponent->SwitchTargetWithDirection에 위임한다.
	 */
	void OnSwitchTarget(const FInputActionValue& Value);

	// ── 공격 입력 콜백 ──────────────────────────────────────────────

	/**
	 * AttackAction(Started) 콜백 — IA_Attack 누르는 순간.
	 * AttackComponent->RequestAttack(PrimaryTap)에 위임한다.
	 * Sprint 태그 보유 중이면 Running, 아니면 Light로 분기(컴포넌트 내부 처리).
	 * 피격/사망 중에는 차단한다.
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

	/**
	 * BlockAction(Started) 콜백 — IA_Block 버튼 누르는 순간.
	 * CanBlock() 확인 후 Blocking 상태 태그 부착 및 이동 속도 감소를 적용한다.
	 */
	void BlockingStart(const FInputActionValue& Value);

	/**
	 * BlockAction(Completed/Canceled) 콜백 — IA_Block 버튼 뗄 때.
	 * Blocking 상태 태그를 해제하고 이동 속도를 WalkSpeed로 복원한다.
	 */
	void BlockingEnd(const FInputActionValue& Value);

	/**
	 * ParryAction(Started) 콜백 — IA_Parry 버튼 누르는 순간.
	 * CanParry() 확인 후 패링 몽타주 재생 및 스태미나 소비.
	 */
	void OnParry(const FInputActionValue& Value);

	/** JumpAction(Started) 콜백: 구르기/피격/사망 중이 아니면 ACharacter::Jump를 호출한다. */
	void StartJump();

	/**
	 * 현재 구르기(Character.State.Roll) 상태인지 확인한다. 이동·점프 차단 판정에 사용.
	 * BP에서도 호출 가능(예: 공격 시작 전 "구르기 중이면 공격 금지" 게이트).
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Roll")
	bool IsRolling() const;

	/** 현재 피격(Character.State.Hit) 상태인지 확인한다. 이동·공격·구르기 차단 판정에 사용. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsHit() const;

	/**
	 * 현재 포션 마시기(Character.State.DrinkingPotion) 상태인지 확인한다.
	 * 공격/방어/점프/구르기/패링 차단 게이트에 사용한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Potion")
	bool IsDrinkingPotion() const;

	/**
	 * DrinkAction(IA_Drink) 입력 콜백.
	 * 상태 게이트 확인 후 DrinkMontage를 재생하고 DrinkingPotion 태그를 부착한다.
	 * 실제 회복·스폰은 몽타주 노티파이가 담당한다.
	 */
	void OnDrinkPotion(const FInputActionValue& Value);

	// ── Sprint / Roll 입력 콜백 ─────────────────────────────────────

	/**
	 * SprintAction(Hold 전용 IA) Triggered 바인딩.
	 * Hold 임계 시간 충족 시 1회 호출된다. 조건 충족 시 질주를 시작한다.
	 */
	void StartSprint(const FInputActionValue& Value);

	/** SprintAction Completed/Canceled 바인딩. 질주를 종료한다. */
	void StopSprint(const FInputActionValue& Value);

	/**
	 * RollAction(Tap 전용 IA) Triggered 바인딩.
	 * 짧은 탭 입력 충족 시 Roll 상태 태그를 부착하고 RollMontage를 재생한다.
	 * 상태 해제는 몽타주에 배치한 AnimNotify(→ EndRoll)가 담당한다.
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

	/**
	 * OnDeath 구독 콜백. 체력 0 도달 시 호출된다.
	 * 입력을 차단하고 DeathMontage를 재생한다(미설정 시 랙돌 폴백).
	 * UFUNCTION 필수(AddDynamic 요구).
	 */
	UFUNCTION()
	void HandleDeath();

	/** 질주를 시작할 수 있는 조건을 확인한다. */
	bool CanSprint() const;

protected:
	// ── 기본 신체 메시 (방어구 장착 시 숨김/복원 대상) ─────────────

	/**
	 * 상체(Torso) 기본 신체 메시. Chest 방어구 장착 시 SetVisibility(false)로 숨긴다.
	 * 메인 메시와 같은 스켈레톤을 공유해야 LeaderPose가 올바르게 작동한다.
	 * BP 자식의 (상속됨) TorsoMesh 컴포넌트에서 SK_ 에셋을 지정한다(새 컴포넌트 추가 금지).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<USkeletalMeshComponent> TorsoMesh;

	/**
	 * 하체(Legs/Pants) 기본 신체 메시. Pants 방어구 장착 시 숨긴다.
	 * BP 자식의 (상속됨) LegsMesh 컴포넌트에서 SK_ 에셋을 지정한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<USkeletalMeshComponent> LegsMesh;

	/**
	 * 발(Feet/Boots) 기본 신체 메시. Boots 방어구 장착 시 숨긴다.
	 * BP 자식의 (상속됨) FeetMesh 컴포넌트에서 SK_ 에셋을 지정한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<USkeletalMeshComponent> FeetMesh;

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

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> RollAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ToggleWeaponAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ToggleShieldAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> HeavyAttackAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LockOnAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> SwitchTargetAction;

	/** 블로킹(가드) 입력 액션. BP 자식에서 IA_Block 에셋을 지정한다. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> BlockAction;

	/** 패링 입력 액션. BP 자식에서 IA_Parry 에셋을 지정한다(IMC 바인딩은 사용자가 직접 설정). */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ParryAction;

	/** 포션 마시기 입력 액션. BP 자식에서 IA_Drink 에셋을 지정한다. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> DrinkAction;

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

	/**
	 * 락온 타겟팅 컴포넌트. 후보 수집·타깃 선택·카메라 보간·스트레이프 전환을 담당한다.
	 * BP 자식의 "(상속됨)" 컴포넌트에서 거리/각도/보간속도 등을 튜닝한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBWTargetingComponent> TargetingComponent;

	/** 포션 인벤토리 컴포넌트. 수량·회복·스폰·HUD 델리게이트를 담당한다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UBWPotionInventoryComponent> PotionInventoryComponent;

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

	/** 회피(Roll) 시 재생할 몽타주. BP 자식에서 AM_ 회피 몽타주를 지정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Roll")
	TObjectPtr<UAnimMontage> RollMontage;

	/** 포션 마시기 몽타주. BP 자식에서 AM_DrinkPotion을 지정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Potion")
	TObjectPtr<UAnimMontage> DrinkMontage;

	// ── Block(가드) 데이터 (BP 자식에서 설정) ────────────────────────

	/** 블로킹 중 이동 속도(cm/s). BlockingStart 시 적용되고, BlockingEnd 시 WalkSpeed로 복원된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Block", meta = (ClampMin = "0.0"))
	float BlockingSpeed = 150.f;

	/** 가드를 발동하기 위해 필요한 최소 스태미나. 미만이면 가드 피격 판정을 허용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Block", meta = (ClampMin = "0.0"))
	float BlockMinStamina = 20.f;

	/**
	 * 가드 성공으로 판정할 전방 콘 임계 내적(코사인). 공격자가 플레이어 정면 기준 이 각도 안에 있어야 가드된다.
	 * 0.0 = 전방 180°(앞쪽 절반), 0.5 ≈ 전방 120°, 0.707 ≈ 전방 90°. 값이 클수록 좁은 정면만 가드.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Block", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float BlockFrontDotThreshold = 0.f;

	/** 블로킹 성공 피격 시 ImpactPoint에 재생할 Cascade 파티클 VFX. BP 자식에서 P_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|BlockEffects")
	TObjectPtr<UParticleSystem> BlockHitVFX;

	/** 블로킹 성공 피격 시 ImpactPoint에 재생할 사운드. BP 자식에서 SW_/SC_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|BlockEffects")
	TObjectPtr<USoundBase> BlockHitSound;

	/** 구르기 1회에 소비하는 스태미나. 부족하면 구르기가 발동하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Roll", meta = (ClampMin = "0.0"))
	float RollStaminaCost = 20.f;

	// ── Block(가드) 데이터 — 패링(Parry) 확장 ─────────────────────────────

	/** 패링 1회 소비 스태미나. 부족하면 패링 발동 불가. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Parry", meta = (ClampMin = "0.0"))
	float ParryStaminaCost = 10.f;

	/** 패링 발동 후 스태미나 리젠이 재개되기까지의 지연(초). AttributeComponent.RegenDelay와 별도로 참고값으로 노출. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Parry", meta = (ClampMin = "0.0"))
	float ParryRegenDelay = 1.5f;

	/**
	 * 패링 성공으로 판정할 전방 콘 임계 내적(코사인). 공격한 적이 정면 기준 이 각도 안에 있어야 패링 성공.
	 * 0.0 = 전방 180°(앞쪽 절반), 0.5 ≈ 전방 120°, 0.707 ≈ 전방 90°.
	 * BlockFrontDotThreshold와 동일 의미이며 패링 전용 임계값으로 별도 관리한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Parry", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float ParryFrontDotThreshold = 0.f;

	/** 패링 성공 시 적 무기 위치에 재생할 Cascade 파티클(방패 막기 연출). BP 자식에서 P_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|ParryEffects")
	TObjectPtr<UParticleSystem> ParryHitVFX;

	/** 패링 성공 시 적 무기 위치에 재생할 사운드. BP 자식에서 SW_/SC_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|ParryEffects")
	TObjectPtr<USoundBase> ParryHitSound;

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

	/** 스태미나 0 고갈 후 자동 재개 기준값. 이 값 이상 회복되면 버튼이 눌린 상태일 때 자동으로 질주를 재개한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Sprint", meta = (ClampMin = "0.0"))
	float SprintResumeThreshold = 15.f;

	// ── Sprint 런타임 상태 ──────────────────────────────────────────

	/** 현재 질주 중인지 여부(에디터 디버그용). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement|Sprint")
	bool bIsSprinting = false;

	// ── BP 설정용 — 4방향 히트 리액션 몽타주 (Enemy 패턴 이식) ────

	/**
	 * 넉다운(쓰러짐→기상) 몽타주. UBWDamageType_Knockdown 피격 시 일반 히트 리액션 대신 재생한다.
	 * null이면 일반 4방향 히트 리액션으로 폴백한다.
	 * BP 자식(BP_PlayerCharacter)에서 AM_ 넉다운 몽타주를 지정한다.
	 * GC 추적: UAnimMontage는 UObject 파생이므로 TObjectPtr + UPROPERTY 필수(CLAUDE.md 3.3).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|HitReaction")
	TObjectPtr<UAnimMontage> KnockdownMontage;

	/**
	 * 기상(일어나기) 몽타주. 넉다운 몽타주가 끝나면(EndKnockdown) 이어서 재생한다.
	 * 이 몽타주가 끝나는 시점(EndGetUp)에 KnockdownHit 태그가 해제되고 입력이 복원된다.
	 * null이면 기상 연출 없이 즉시 EndGetUp으로 넘어가 입력을 돌려준다(입력 먹통 방지).
	 * BP 자식(BP_PlayerCharacter)에서 AM_ 기상 몽타주를 지정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|HitReaction")
	TObjectPtr<UAnimMontage> GetUpMontage;

	/** 정면 피격 히트 리액션 몽타주. BP 자식에서 AM_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|HitReaction")
	TObjectPtr<UAnimMontage> HitReactFrontMontage;

	/** 후방 피격 히트 리액션 몽타주. BP 자식에서 AM_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|HitReaction")
	TObjectPtr<UAnimMontage> HitReactBackMontage;

	/** 왼쪽 피격 히트 리액션 몽타주. BP 자식에서 AM_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|HitReaction")
	TObjectPtr<UAnimMontage> HitReactLeftMontage;

	/** 오른쪽 피격 히트 리액션 몽타주. BP 자식에서 AM_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|HitReaction")
	TObjectPtr<UAnimMontage> HitReactRightMontage;

	// ── BP 설정용 — 피격 VFX·사운드 ──────────────────────────────

	/** 피격 Cascade 파티클 VFX. ImpactPoint에 스폰된다. BP 자식에서 P_ 에셋 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|HitEffects")
	TObjectPtr<UParticleSystem> HitVFX;

	/** 피격 사운드(MetaSound/SoundWave 등 USoundBase 호환). ImpactPoint에 재생. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|HitEffects")
	TObjectPtr<USoundBase> HitSound;

	// ── BP 설정용 — 사망 ──────────────────────────────────────────

	/**
	 * 사망 시 재생할 몽타주. BP 자식에서 AM_ 에셋 지정.
	 * null이면 랙돌로 폴백한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Death")
	TObjectPtr<UAnimMontage> DeathMontage;

	// ── 런타임 상태 ─────────────────────────────────────────────────

	/**
	 * 무적(i-frame) 상태 여부. true이면 TakeDamage가 데미지·히트 리액션·이펙트 없이 조기 반환한다.
	 * UBWAnimNotifyState_Invincibility가 구르기 몽타주 윈도우에서 ON/OFF 한다.
	 * 코드가 소유하는 런타임 상태 — 디버깅 관찰용으로만 노출.
	 * 안전망: EndRoll / OnRollMontageEnded에서도 강제 해제해 영구 무적을 방지한다.
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat|Invincibility")
	bool bIsInvincible = false;

	/** 사망 여부. TakeDamage 재진입 차단 및 HandleDeath 중복 진입 방지. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|Death")
	bool bIsDead = false;

private:
	/** Sprint 버튼이 눌린 채로 유지되고 있는지 여부. 자동 재개 판정에 사용. */
	bool bSprintInputHeld = false;

	/** 블로킹 시작 전 이동 속도 백업값. BlockingEnd 시 MaxWalkSpeed 복원에 사용. */
	float SpeedBeforeBlocking = 0.f;

	/** 스태미나 소모 반복 타이머 핸들. */
	FTimerHandle SprintDrainTimerHandle;

	/**
	 * 락온 상태 캐시 bool. AnimInstance(워커 스레드)가 IsLockedOn() 대신 이 값을 읽어
	 * WeakPtr 유효성 검사 없이 안전하게 락온 여부를 조회한다.
	 * TargetingComponent가 StartLockOn/EndLockOn 시 SetIsLockedOnCached를 통해 갱신한다.
	 */
	bool bIsLockedOnCached = false;

	/**
	 * 구르기 시작 시 락온 스트레이프 상태라 bUseControllerRotationYaw를 일시 비활성화했는지 여부.
	 * EndRoll에서 (락온 유지 중일 때만) 다시 true로 복원하는 데 사용한다.
	 */
	bool bRestoreControllerYawAfterRoll = false;

	/**
	 * 패링 몽타주가 진행 중인지 여부. OnParry에서 몽타주 재생이 확정되면 true로 설정하고,
	 * OnParryMontageEnded(정상 종료/중단 모두 호출 보장)에서 false로 되돌린다.
	 * Character.State.Parrying 태그는 패링 윈도우(NotifyState) 구간에만 켜지므로,
	 * 윈도우 전/후 회복 구간을 포함한 몽타주 전체 동안 재입력을 막기 위해 별도 플래그로 추적한다.
	 * CanParry에서 이 값이 true이면 재발동을 차단해 "한 번만 발동"을 보장한다.
	 */
	bool bIsPerformingParry = false;

	/**
	 * DrinkMontage 종료 안전망 콜백(Montage_SetEndDelegate 바인딩).
	 * 노티파이 누락·중단 시에도 DrinkingPotion 태그 해제 + 포션 디스폰을 보장한다(중복 호출 무해).
	 */
	void OnDrinkMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * 현재 플레이어가 입력 중인 이동 방향(수평 월드 벡터, 정규화)을 반환한다.
	 * Move()가 컨트롤러 Yaw 기준으로 변환해 넣은 입력을 CharacterMovement에서 읽으므로 카메라 상대 방향이다.
	 * 이번 프레임 누적 입력(미소비)을 우선 사용하고, 비어 있으면 직전 프레임 소비 입력으로 폴백한다.
	 * 입력이 없으면 ZeroVector. 구르기 방향 결정에 사용한다.
	 */
	FVector GetRollInputDirection() const;

	// ── 패링(Parry) 헬퍼 ────────────────────────────────────────────

	/**
	 * 무기 DataTable의 Parrying 행에서 패링 몽타주를 조회한다.
	 * 무기/DataTable/행/Steps 없으면 nullptr 반환. GetBlockingHitMontage 패턴 동일.
	 */
	UAnimMontage* GetParryMontage() const;

	/**
	 * 패링 성공 시 지정 위치에 VFX/사운드를 재생한다.
	 * PlayBlockingHitReaction의 이펙트 재생 부분과 동일 패턴.
	 * @param Location VFX/사운드를 재생할 월드 위치(적 무기 위치).
	 */
	void PlayParrySuccessEffects(const FVector& Location);

	/**
	 * 패링 몽타주 종료 콜백(안전망).
	 * NotifyEnd가 이미 Parrying 태그를 해제했더라도, 누락/중단 대비 RemoveStateTag를 보장 호출한다.
	 */
	void OnParryMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// ── 블로킹 히트 리액션 헬퍼 ────────────────────────────────────

	/**
	 * 무기 DataTable의 BlockingHit 행에서 몽타주를 조회한다.
	 * 무기/DataTable/행/Steps 없으면 nullptr 반환.
	 */
	UAnimMontage* GetBlockingHitMontage() const;

	/**
	 * 가드 성공 시 블로킹 히트 리액션 몽타주를 재생하고 ImpactPoint에 이펙트/사운드를 스폰한다.
	 * BlockingHit 태그를 부착해 입력을 잠그고, 몽타주 종료 시 자동 해제된다.
	 */
	void PlayBlockingHitReaction(const FVector& ImpactPoint);

	/**
	 * 블로킹 히트 몽타주 종료 콜백. Character_Action_BlockingHit 태그를 해제한다(안전망).
	 * 정상 종료든 중단이든 BlockingHit 상태가 남아 입력이 먹통이 되지 않도록 보장.
	 */
	void OnBlockingHitMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * 플레이어 입력을 잠그거나(true) 복원한다(false).
	 * PlayerController 단위(SetIgnoreMoveInput/SetIgnoreLookInput + DisableInput)로 처리한다.
	 * 이전 이름 SetBlockingHitInputLocked에서 일반화 — 블로킹 히트/넉다운 양쪽 경로가 공유한다.
	 * 잠금/복원은 각 경로의 진입 가드(IsBlockingHit/IsKnockedDown)가 정확히 1쌍을 보장한다.
	 */
	void SetInputLocked(bool bLocked);

	/**
	 * 넉다운 리액션을 시작한다. KnockdownHit 태그 부착 + 입력 잠금 + KnockdownMontage 재생.
	 * 재진입 가드(IsKnockedDown)로 Montage_SetEndDelegate 교체에 의한 콜백 유실을 방지한다.
	 * Montage_Play 반환 <= 0이면 즉시 EndKnockdown()을 호출해 입력 먹통을 방지한다.
	 * TakeDamage의 일반 피격 경로에서 bIsKnockdownDamage && KnockdownMontage 조건 충족 시 호출한다.
	 */
	void PlayKnockdownReaction();

	/**
	 * KnockdownMontage의 블렌드 아웃 시작 콜백(Montage_SetBlendingOutDelegate)이자
	 * 종료 콜백(Montage_SetEndDelegate 안전망) — 두 델리게이트에 함께 바인딩한다.
	 * 블렌드 아웃 시작 시점에 EndKnockdown()으로 기상 몽타주를 이어 재생해야
	 * 넉다운 → 아이들 → 기상으로 끊기지 않고 한 동작으로 연결된다.
	 * 중단(bInterrupted=true)이면 기상을 덧씌우지 않고 EndGetUp()으로 상태만 해제한다.
	 * bIsGettingUp 가드가 중복 호출(BlendingOut → End 연속 발화, AnimNotify 선행 호출)을 차단한다.
	 */
	void OnKnockdownMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * GetUpMontage 종료 콜백(Montage_SetEndDelegate 안전망).
	 * 정상 종료든 중단이든 EndGetUp()을 호출해 입력이 영구히 잠기지 않도록 보장한다.
	 * AnimNotify가 먼저 EndGetUp을 호출했다면 IsKnockedDown 가드가 중복 복원을 차단한다.
	 */
	void OnGetUpMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * 기상 몽타주 재생 중인지 나타내는 플래그. EndKnockdown의 재진입 가드로 사용한다.
	 * 기상 구간에도 KnockdownHit 태그가 유지되므로 IsKnockedDown()만으로는 중복 진입을 막을 수 없다
	 * (재진입 시 Montage_SetEndDelegate가 교체되어 종료 콜백이 유실 → 입력 영구 잠김).
	 */
	bool bIsGettingUp = false;

	/**
	 * 블로킹(가드) 상태를 취소한다. Blocking 태그를 해제하고 이동 속도를 복원한다.
	 * BlockingEnd(버튼 뗌)와 구르기 시작(Roll)에서 공통으로 호출한다.
	 * 블로킹 중이 아니면 무동작. 구르기가 가드를 풀어주는 경로는 입력 엣지에 의존하지 않는다.
	 */
	void CancelBlocking();

	/**
	 * 공격자가 플레이어 정면 콘(BlockFrontDotThreshold) 안에 있는지 판정한다.
	 * 기존 가드 경로에서 사용한다(BlockFrontDotThreshold를 내부적으로 읽음).
	 * Attacker가 null이면 false.
	 */
	bool IsAttackerInFront(const AActor* Attacker) const;

	/**
	 * 공격자가 플레이어 정면 콘 안에 있는지 판정한다. 임계값을 인자로 받는 오버로드.
	 * 패링 경로에서 ParryFrontDotThreshold를 전달해 사용한다.
	 * 기존 IsAttackerInFront(Attacker)는 이 오버로드에 BlockFrontDotThreshold를 전달해 위임한다.
	 */
	bool IsAttackerInFront(const AActor* Attacker, float FrontDotThreshold) const;

	// ── 피격 처리 헬퍼 (Enemy 패턴 복제) ───────────────────────────

	/**
	 * 공격자의 실제 위치와 피격자 forward/right 내적으로 4방향을 분류한다.
	 * Front(±45°) / Back / Left / Right. Attacker가 null이면 Front로 폴백.
	 * (무기 스윙 벡터가 아닌 공격자 위치 기준 — 가로 베기 등에서도 방향이 올바르다.)
	 */
	EBWHitDirection ComputeHitDirection(const AActor* Attacker) const;

	/**
	 * 분류된 방향에 따라 해당 히트 리액션 몽타주를 재생하고 Character_State_Hit 태그를 부착한다.
	 * 몽타주 종료 시 Character_State_Hit 태그를 제거하는 안전망 콜백을 바인딩한다.
	 * 해당 방향 몽타주가 null이면 Front로 폴백, Front도 null이면 Warning.
	 */
	void PlayHitReaction(const AActor* Attacker);

	/**
	 * 히트 리액션 몽타주 종료 콜백. Character_State_Hit 태그를 제거한다.
	 * 안전망: 정상 종료든 중단이든 Hit 상태가 남아 입력이 먹통이 되지 않도록 보장.
	 */
	void OnHitReactionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/**
	 * ImpactPoint에 Cascade 파티클 VFX 스폰 + 사운드 재생.
	 * HitVFX/HitSound가 설정되지 않으면 조용히 건너뛴다.
	 */
	void PlayHitEffects(const FVector& ImpactPoint);

	/**
	 * 사망 처리. 사망 연출(DeathMontage) 또는 랙돌 폴백.
	 * 내부적으로 HandleDeath에서 호출된다.
	 */
	void EnableDeathState();
};
