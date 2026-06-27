// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/BWPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/DamageEvents.h"
#include "Particles/ParticleSystem.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Combat/BWAttributeComponent.h"
#include "Combat/BWCombatComponent.h"
#include "Combat/BWAttackComponent.h"
#include "Combat/BWTargetingComponent.h"
#include "Combat/BWAttackTypes.h"
#include "Character/BWStateComponent.h"
#include "Core/BWGameplayDefine.h"
#include "Equipment/BWPickUpItem.h"
#include "Equipment/BWEquipItem.h"
#include "Equipment/BWArmour.h"
#include "DrawDebugHelpers.h"

ABWPlayerCharacter::ABWPlayerCharacter()
{
	// 전투 로직은 입력/애님 노티파이 이벤트 기반으로 처리한다. 매 프레임 틱은 비활성.
	PrimaryActorTick.bCanEverTick = false;

	// 카메라 붐: 캐릭터 뒤로 거리를 두고 레벨 지오메트리에 막히면 자동으로 당겨진다.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->SetRelativeRotation(FRotator(-30.0f, 0.0f, 0.0f));
	CameraBoom->bUsePawnControlRotation = true;

	// 팔로우 카메라: 붐 끝에 부착, 회전은 붐이 담당하므로 카메라 자체는 컨트롤 회전 미사용.
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 소울라이크 이동: 컨트롤러 회전을 캐릭터에 직접 적용하지 않고 이동 방향으로 회전.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
		// 기본 보행 속도를 WalkSpeed와 일치시킨다.
		Movement->MaxWalkSpeed = WalkSpeed;
	}

	// AttributeComponent 생성·부착 (GC 추적을 위해 UPROPERTY + TObjectPtr)
	AttributeComponent = CreateDefaultSubobject<UBWAttributeComponent>(TEXT("AttributeComponent"));

	// StateComponent 생성·부착. GameplayTagContainer로 행동 상태를 관리한다.
	StateComponent = CreateDefaultSubobject<UBWStateComponent>(TEXT("StateComponent"));

	// CombatComponent 생성·부착. 장비 보유·장착·해제 로직을 담당한다.
	CombatComponent = CreateDefaultSubobject<UBWCombatComponent>(TEXT("CombatComponent"));

	// AttackComponent 생성·부착. 공격(콤보) 로직을 담당한다.
	AttackComponent = CreateDefaultSubobject<UBWAttackComponent>(TEXT("AttackComponent"));

	// TargetingComponent 생성·부착. 락온 타겟팅 로직을 담당한다. Tick은 기본 비활성.
	TargetingComponent = CreateDefaultSubobject<UBWTargetingComponent>(TEXT("TargetingComponent"));

	// ── 기본 신체 메시 생성 ─────────────────────────────────────────
	// 방어구 장착 시 숨기고, 해제 시 다시 표시하는 부위별 신체 파츠.
	// LeaderPose는 BeginPlay에서 안전하게 재설정한다(생성자 시점엔 월드/플레이어 참조 불안정).
	// BP 자식의 (상속됨) 컴포넌트에서 SK_ 에셋을 지정해야 한다(새 컴포넌트 추가 금지).

	TorsoMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TorsoMesh"));
	TorsoMesh->SetupAttachment(GetMesh());
	TorsoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TorsoMesh->SetSimulatePhysics(false);

	LegsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LegsMesh"));
	LegsMesh->SetupAttachment(GetMesh());
	LegsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LegsMesh->SetSimulatePhysics(false);

	FeetMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FeetMesh"));
	FeetMesh->SetupAttachment(GetMesh());
	FeetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FeetMesh->SetSimulatePhysics(false);
}

void ABWPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// AttributeComponent 델리게이트 구독
	if (ensure(IsValid(AttributeComponent)))
	{
		AttributeComponent->OnStaminaDepleted.AddDynamic(this, &ABWPlayerCharacter::HandleStaminaDepleted);
		AttributeComponent->OnStaminaChanged.AddDynamic(this, &ABWPlayerCharacter::HandleStaminaChanged);
		AttributeComponent->OnDeath.AddDynamic(this, &ABWPlayerCharacter::HandleDeath);
	}

	// 신체 메시 LeaderPose 재설정.
	// 생성자에서 attach 후 BeginPlay에서 다시 설정해 에디터 리로드/PIE 전환 안정성을 높인다.
	// 메인 메시와 같은 스켈레톤을 공유하는 SK_ 에셋이 지정되어 있어야 본 추종이 올바르게 작동한다.
	if (USkeletalMeshComponent* MainMesh = GetMesh())
	{
		if (TorsoMesh)
		{
			TorsoMesh->SetLeaderPoseComponent(MainMesh);
		}
		if (LegsMesh)
		{
			LegsMesh->SetLeaderPoseComponent(MainMesh);
		}
		if (FeetMesh)
		{
			FeetMesh->SetLeaderPoseComponent(MainMesh);
		}
	}
}

void ABWPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 스태미나 소모 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SprintDrainTimerHandle);
	}

	// 델리게이트 구독 해제 (댕글링 콜백 방지 — 규약 4.3)
	if (IsValid(AttributeComponent))
	{
		AttributeComponent->OnStaminaDepleted.RemoveDynamic(this, &ABWPlayerCharacter::HandleStaminaDepleted);
		AttributeComponent->OnStaminaChanged.RemoveDynamic(this, &ABWPlayerCharacter::HandleStaminaChanged);
		AttributeComponent->OnDeath.RemoveDynamic(this, &ABWPlayerCharacter::HandleDeath);
	}

	Super::EndPlay(EndPlayReason);
}

void ABWPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void ABWPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABWPlayerCharacter::Move);
		}

		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABWPlayerCharacter::Look);
		}

		if (JumpAction)
		{
			// 점프 시작은 구르기 차단을 위해 래퍼(StartJump)를 경유한다. 종료는 ACharacter 내장 구현 사용.
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::StartJump);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}

		if (SprintAction)
		{
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Triggered,  this, &ABWPlayerCharacter::StartSprint);
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed,  this, &ABWPlayerCharacter::StopSprint);
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Canceled,   this, &ABWPlayerCharacter::StopSprint);
		}

		if (RollAction)
		{
			EnhancedInput->BindAction(RollAction, ETriggerEvent::Triggered, this, &ABWPlayerCharacter::Roll);
		}

		if (InteractAction)
		{
			EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::Interact);
		}

		if (ToggleWeaponAction)
		{
			EnhancedInput->BindAction(ToggleWeaponAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::ToggleWeapon);
		}

		if (ToggleShieldAction)
		{
			EnhancedInput->BindAction(ToggleShieldAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::ToggleShield);
		}

		if (AttackAction)
		{
			// IA_Attack: Started(누르는 순간) → PrimaryTap(Light/Running 분기), Triggered(Hold 충족) → PrimaryHold(Special).
			EnhancedInput->BindAction(AttackAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::OnAttackStarted);
			EnhancedInput->BindAction(AttackAction, ETriggerEvent::Triggered, this, &ABWPlayerCharacter::OnAttackHold);
		}

		if (HeavyAttackAction)
		{
			EnhancedInput->BindAction(HeavyAttackAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::OnHeavyAttack);
		}

		if (LockOnAction)
		{
			EnhancedInput->BindAction(LockOnAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::OnLockOn);
		}

		if (SwitchTargetAction)
		{
			EnhancedInput->BindAction(SwitchTargetAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::OnSwitchTarget);
		}
	}
}

// ── 신체 메시 가시성 제어 ────────────────────────────────────────────────────

void ABWPlayerCharacter::SetBodyArmourHidden(EBWArmourType Type, bool bHideBodyPart)
{
	switch (Type)
	{
	case EBWArmourType::Chest:
		if (TorsoMesh)
		{
			TorsoMesh->SetVisibility(!bHideBodyPart);
		}
		break;

	case EBWArmourType::Pants:
		if (LegsMesh)
		{
			LegsMesh->SetVisibility(!bHideBodyPart);
		}
		break;

	case EBWArmourType::Boots:
		if (FeetMesh)
		{
			FeetMesh->SetVisibility(!bHideBodyPart);
		}
		break;

	case EBWArmourType::Gloves:
	default:
		// Gloves는 대응 기본 신체 메시가 없다 — 크래시 없이 무시.
		break;
	}
}

// ── IBWCombatInterface 구현 ───────────────────────────────────────────────────

void ABWPlayerCharacter::PerformAttack(FOnMontageEnded OnEnded)
{
	// 플레이어는 BT가 직접 호출하지 않는다 — 인터페이스 계약 충족용 얇은 구현.
	// AttackComponent->RequestAttack(PrimaryTap)에 위임한다. OnEnded는 미사용.
	if (IsValid(AttackComponent))
	{
		AttackComponent->RequestAttack(EBWAttackInputKind::PrimaryTap);
	}
}

// ── TakeDamage 오버라이드 ─────────────────────────────────────────────────────

float ABWPlayerCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// 이미 사망한 플레이어는 추가 타격을 무시한다.
	if (bIsDead)
	{
		return 0.f;
	}

	// 엔진 표준 처리를 통해 실제 데미지 수치를 획득한다.
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// FPointDamageEvent에서 ImpactPoint/ShotDirection 추출
	FVector ImpactPoint = GetActorLocation();
	FVector ShotDirection = GetActorForwardVector(); // 폴백: 정면에서 맞은 것으로 처리

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		ImpactPoint = PointEvent.HitInfo.ImpactPoint;

		if (!PointEvent.ShotDirection.IsNearlyZero())
		{
			ShotDirection = PointEvent.ShotDirection;
		}
	}

	// 방어 공식 적용: 방어력을 반영한 최종 데미지를 계산한 후 AttributeComponent에 위임.
	// DefenseStat = 0이면 CalculateMitigatedDamage가 ActualDamage를 그대로 반환한다(방어력 없을 때 패널티 없음).
	if (AttributeComponent)
	{
		const float MitigatedDamage = AttributeComponent->CalculateMitigatedDamage(ActualDamage);
		AttributeComponent->ApplyDamage(MitigatedDamage);
	}

	// 피격 이펙트·리액션 재생 (사망은 OnDeath 델리게이트가 별도 트리거)
	PlayHitEffects(ImpactPoint);
	PlayHitReaction(ShotDirection);

	return ActualDamage;
}

// ── 사망 처리 ────────────────────────────────────────────────────────────────

void ABWPlayerCharacter::HandleDeath()
{
	// bIsDead 가드 — HandleDeath 재진입 방지
	if (bIsDead)
	{
		return;
	}

	EnableDeathState();
}

void ABWPlayerCharacter::EnableDeathState()
{
	// 1) 사망 플래그 설정
	bIsDead = true;

	// 2) 행동 상태 태그 모두 해제 후 Death 태그 부착
	if (IsValid(StateComponent))
	{
		StateComponent->ClearAllStateTags();
		StateComponent->AddStateTag(BWGameplayTags::Character_State_Death.GetTag());
	}

	// 3) 진행 중인 몽타주 정지
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		AnimInstance->Montage_Stop(0.1f);
	}

	// 4) 입력 차단 — 플레이어 컨트롤러에서 입력을 비활성화한다.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
		DisableInput(PC);
	}

	// 5) 이동 중단
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	// 6) 사망 연출: DeathMontage 재생 (없으면 랙돌 폴백)
	if (DeathMontage && AnimInstance)
	{
		AnimInstance->Montage_Play(DeathMontage);
	}
	else
	{
		// 랙돌 폴백 — DeathMontage가 지정되지 않은 경우
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->DisableMovement();
		}

		if (USkeletalMeshComponent* PlayerMesh = GetMesh())
		{
			PlayerMesh->SetCollisionProfileName(TEXT("Ragdoll"));
			PlayerMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			PlayerMesh->SetSimulatePhysics(true);
		}
	}
}

// ── 입력 콜백 ────────────────────────────────────────────────────────────────

void ABWPlayerCharacter::Move(const FInputActionValue& Value)
{
	// 구르기, 공격, 피격, 사망 중에는 이동 입력을 무시한다.
	if (IsRolling() || IsAttacking() || IsHit() || bIsDead)
	{
		return;
	}

	const FVector2D MovementVector = Value.Get<FVector2D>();

	AController* PlayerController = GetController();
	if (!PlayerController)
	{
		return;
	}

	// 카메라(컨트롤러) Yaw 기준으로 전/후·좌/우 방향을 구해 이동한다. Pitch/Roll은 평면 이동에서 제외.
	const FRotator YawRotation(0.0f, PlayerController->GetControlRotation().Yaw, 0.0f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MovementVector.Y);
	AddMovementInput(RightDirection, MovementVector.X);
}

void ABWPlayerCharacter::Look(const FInputActionValue& Value)
{
	if (IsLockedOn())
	{
		return;
	}

	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void ABWPlayerCharacter::StartJump()
{
	// 구르기, 공격, 피격, 사망 중에는 점프를 차단한다.
	if (IsRolling() || IsAttacking() || IsHit() || bIsDead)
	{
		return;
	}

	Jump();
}

bool ABWPlayerCharacter::IsRolling() const
{
	return IsValid(StateComponent) && StateComponent->HasStateTag(BWGameplayTags::Character_State_Roll.GetTag());
}

bool ABWPlayerCharacter::IsHit() const
{
	return IsValid(StateComponent) && StateComponent->HasStateTag(BWGameplayTags::Character_State_Hit.GetTag());
}

// ── Sprint 입력 콜백 ─────────────────────────────────────────────────────────

void ABWPlayerCharacter::StartSprint(const FInputActionValue& Value)
{
	bSprintInputHeld = true;

	if (CanSprint())
	{
		BeginSprinting();
	}
}

void ABWPlayerCharacter::StopSprint(const FInputActionValue& Value)
{
	bSprintInputHeld = false;
	EndSprinting();
}

// ── Sprint 상태 전이 ─────────────────────────────────────────────────────────

void ABWPlayerCharacter::BeginSprinting()
{
	if (bIsSprinting)
	{
		return;
	}

	bIsSprinting = true;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = SprintSpeed;
	}

	// 스태미나 소모 반복 타이머 시작
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SprintDrainTimerHandle,
			this,
			&ABWPlayerCharacter::TickSprintDrain,
			SprintStaminaDrainInterval,
			/*bLoop=*/true
		);
	}

	// Sprint 상태 태그 부착
	if (StateComponent)
	{
		StateComponent->AddStateTag(BWGameplayTags::Character_State_Sprint.GetTag());
	}
}

void ABWPlayerCharacter::EndSprinting()
{
	if (!bIsSprinting)
	{
		return;
	}

	bIsSprinting = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = WalkSpeed;
	}

	// 스태미나 소모 타이머 정지
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SprintDrainTimerHandle);
	}

	// Sprint 상태 태그 해제
	if (StateComponent)
	{
		StateComponent->RemoveStateTag(BWGameplayTags::Character_State_Sprint.GetTag());
	}
}

void ABWPlayerCharacter::TickSprintDrain()
{
	if (!IsValid(AttributeComponent))
	{
		EndSprinting();
		return;
	}

	const float DrainAmount = SprintStaminaCostPerSecond * SprintStaminaDrainInterval;
	const bool bSuccess = AttributeComponent->ConsumeStamina(DrainAmount);

	if (!bSuccess)
	{
		// 스태미나 부족 — 질주 중단(ConsumeStamina가 false를 반환할 때는 OnStaminaDepleted가 발생하지 않으므로 여기서 직접 종료).
		EndSprinting();
	}
}

void ABWPlayerCharacter::HandleStaminaDepleted()
{
	if (bIsSprinting)
	{
		// bSprintInputHeld는 건드리지 않음 — 버튼 유지 중이면 임계치 회복 후 자동 재개
		EndSprinting();
	}
}

void ABWPlayerCharacter::HandleStaminaChanged(float NewValue, float MaxValue)
{
	// 버튼이 눌린 상태이고, 현재 질주 중이 아니며, 스태미나가 임계치 이상이면 자동 재개
	if (bSprintInputHeld && !bIsSprinting && IsValid(AttributeComponent))
	{
		if (AttributeComponent->IsStaminaAboveThreshold(SprintResumeThreshold))
		{
			BeginSprinting();
		}
	}
}

void ABWPlayerCharacter::Roll(const FInputActionValue& Value)
{
	if (!IsValid(StateComponent))
	{
		return;
	}

	// 피격, 사망, 이미 구르는 중, 공격 중에는 중복 입력 무시.
	if (IsHit() || bIsDead || IsRolling() || IsAttacking())
	{
		return;
	}

	// 공중(점프/낙하) 중에는 구를 수 없다. 지상에서만 회피를 허용한다.
	if (const UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (Movement->IsFalling())
		{
			return;
		}
	}

	// 회피 몽타주가 없으면 구르기를 진행하지 않는다(상태만 부착되고 해제 못 해 Move/Jump가 영구 차단되는 것 방지).
	if (!RollMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWPlayerCharacter] RollMontage가 설정되지 않아 구르기를 재생할 수 없습니다. BP 자식에서 RollMontage를 지정하세요."));
		return;
	}

	// 스태미나가 부족하면 구르기 불가(소울라이크: 자원 없으면 행동 불가).
	if (IsValid(AttributeComponent) && !AttributeComponent->HasEnoughStamina(RollStaminaCost))
	{
		return;
	}

	// 구르기 상태 진입(Normal 자동 해제). 이 동안 Move/Jump가 차단된다.
	StateComponent->AddStateTag(BWGameplayTags::Character_State_Roll.GetTag());

	// 스태미나 소비.
	if (IsValid(AttributeComponent))
	{
		AttributeComponent->ConsumeStamina(RollStaminaCost);
	}

	// 회피 몽타주 재생.
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	const float MontageLength = AnimInstance ? AnimInstance->Montage_Play(RollMontage) : 0.f;
	if (MontageLength > 0.f)
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ABWPlayerCharacter::OnRollMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, RollMontage);
	}
	else
	{
		// 몽타주 재생 실패 — 상태가 갇히지 않도록 즉시 해제한다.
		EndRoll();
	}
}

void ABWPlayerCharacter::OnRollMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 정상 종료(노티파이가 이미 EndRoll 했을 수 있음)든 중단이든 Roll 상태를 확실히 해제한다.
	EndRoll();
}

void ABWPlayerCharacter::EndRoll()
{
	if (IsValid(StateComponent))
	{
		// Roll 태그 해제 → StateComponent가 행동 상태 부재를 감지해 Normal로 자동 복귀시킨다.
		StateComponent->RemoveStateTag(BWGameplayTags::Character_State_Roll.GetTag());
	}
}

bool ABWPlayerCharacter::IsLockedOn() const
{
	return bIsLockedOnCached;
}

void ABWPlayerCharacter::OnLockOn(const FInputActionValue& /*Value*/)
{
	if (!IsValid(TargetingComponent))
	{
		return;
	}

	TargetingComponent->ToggleLockOn();
}

void ABWPlayerCharacter::OnSwitchTarget(const FInputActionValue& Value)
{
	if (!IsValid(TargetingComponent))
	{
		return;
	}

	if (IsLockedOn() == false)
	{
		return;
	}

	const float AxisX = Value.Get<float>();
	TargetingComponent->SwitchTargetWithDirection(AxisX);
}

bool ABWPlayerCharacter::CanSprint() const
{
	if (!IsValid(AttributeComponent))
	{
		return false;
	}

	const float MinRequired = SprintStaminaCostPerSecond * SprintStaminaDrainInterval;
	return AttributeComponent->HasEnoughStamina(MinRequired);
}

void ABWPlayerCharacter::Interact(const FInputActionValue& /*Value*/)
{
	if (!IsValid(CombatComponent))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 캐릭터 위치에서 전방으로 InteractTraceDistance만큼 구체 스윕.
	const FVector Start = GetActorLocation();
	const FVector End   = Start + GetActorForwardVector() * InteractTraceDistance;

	TArray<FHitResult> Hits;
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(InteractTraceRadius);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BWInteractTrace), false, this);

	const bool bHit = World->SweepMultiByChannel(
		Hits, Start, End, FQuat::Identity, InteractTraceChannel, Sphere, Params);

	if (bDrawInteractDebug)
	{
		DrawDebugSphere(World, End, InteractTraceRadius, 12,
			bHit ? FColor::Green : FColor::Red, false, 1.0f);
	}

	if (!bHit)
	{
		return;
	}

	// 가장 가까운 ABWPickUpItem 선택.
	ABWPickUpItem* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (const FHitResult& H : Hits)
	{
		if (ABWPickUpItem* Pick = Cast<ABWPickUpItem>(H.GetActor()))
		{
			const float DistSq = FVector::DistSquared(Start, Pick->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Pick;
			}
		}
	}

	if (!Best)
	{
		return;
	}

	const FTransform BestTransform = Best->GetActorTransform();

	if (CombatComponent->EquipNewItem(Best->GetEquipItemClass(), BestTransform))
	{
		if (IsValid(Best))
		{
			Best->Consume();
		}
	}
}

void ABWPlayerCharacter::ToggleWeapon(const FInputActionValue& /*Value*/)
{
	if (!IsValid(CombatComponent))
	{
		return;
	}

	CombatComponent->ToggleWeapon();
}

void ABWPlayerCharacter::ToggleShield(const FInputActionValue& /*Value*/)
{
	if (!IsValid(CombatComponent))
	{
		return;
	}

	CombatComponent->ToggleShield();
}

bool ABWPlayerCharacter::IsAttacking() const
{
	if (!IsValid(AttackComponent))
	{
		return false;
	}

	return AttackComponent->IsAttacking();
}

// ── 공격 입력 콜백 ─────────────────────────────────────────────────────────────

void ABWPlayerCharacter::OnAttackStarted(const FInputActionValue& /*Value*/)
{
	// 피격 또는 사망 중에는 공격 입력 차단
	if (IsHit() || bIsDead)
	{
		return;
	}

	if (!IsValid(AttackComponent))
	{
		return;
	}

	AttackComponent->RequestAttack(EBWAttackInputKind::PrimaryTap);
}

void ABWPlayerCharacter::OnAttackHold(const FInputActionValue& /*Value*/)
{
	// 피격 또는 사망 중에는 공격 입력 차단
	if (IsHit() || bIsDead)
	{
		return;
	}

	if (!IsValid(AttackComponent))
	{
		return;
	}

	AttackComponent->RequestAttack(EBWAttackInputKind::PrimaryHold);
}

void ABWPlayerCharacter::OnHeavyAttack(const FInputActionValue& /*Value*/)
{
	// 피격 또는 사망 중에는 공격 입력 차단
	if (IsHit() || bIsDead)
	{
		return;
	}

	if (!IsValid(AttackComponent))
	{
		return;
	}

	AttackComponent->RequestAttack(EBWAttackInputKind::Heavy);
}

// ── 피격 처리 (Enemy 패턴 이식) ──────────────────────────────────────────────

EBWHitDirection ABWPlayerCharacter::ComputeHitDirection(const FVector& ShotDirection) const
{
	if (ShotDirection.IsNearlyZero())
	{
		return EBWHitDirection::Front;
	}

	// 피격자 → 공격자 방향 (수평 평면 투영)
	FVector FromAttacker = ShotDirection;
	FromAttacker.Z = 0.f;
	if (!FromAttacker.Normalize())
	{
		return EBWHitDirection::Front;
	}

	FVector Fwd = GetActorForwardVector();
	Fwd.Z = 0.f;
	Fwd.Normalize();

	FVector Right = GetActorRightVector();
	Right.Z = 0.f;
	Right.Normalize();

	const float ForwardDot = FVector::DotProduct(Fwd, FromAttacker);
	const float RightDot   = FVector::DotProduct(Right, FromAttacker);

	// COS45 ≈ 0.707 — 앞뒤를 ±45° 콘으로, 나머지를 좌우로 분류
	constexpr float COS45 = 0.707f;

	if (ForwardDot >= COS45)
	{
		return EBWHitDirection::Front;
	}
	else if (ForwardDot <= -COS45)
	{
		return EBWHitDirection::Back;
	}
	else if (RightDot > 0.f)
	{
		return EBWHitDirection::Right;
	}
	else
	{
		return EBWHitDirection::Left;
	}
}

void ABWPlayerCharacter::PlayHitReaction(const FVector& ShotDirection)
{
	// 이미 사망 중이면 히트 리액션을 재생하지 않는다.
	if (bIsDead)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	const EBWHitDirection Direction = ComputeHitDirection(ShotDirection);

	UAnimMontage* MontageToPlay = nullptr;
	switch (Direction)
	{
	case EBWHitDirection::Front: MontageToPlay = HitReactFrontMontage; break;
	case EBWHitDirection::Back:  MontageToPlay = HitReactBackMontage;  break;
	case EBWHitDirection::Left:  MontageToPlay = HitReactLeftMontage;  break;
	case EBWHitDirection::Right: MontageToPlay = HitReactRightMontage; break;
	}

	// 해당 방향 몽타주가 없으면 Front로 폴백
	if (!MontageToPlay)
	{
		MontageToPlay = HitReactFrontMontage;
	}

	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWPlayerCharacter] PlayHitReaction: 히트 리액션 몽타주가 설정되지 않았습니다."));
		return;
	}

	// Character_State_Hit 태그 부착 — 이동·공격·구르기 입력 차단 시작
	if (IsValid(StateComponent))
	{
		StateComponent->AddStateTag(BWGameplayTags::Character_State_Hit.GetTag());
	}

	// 히트 리액션 몽타주 재생
	const float MontageLength = AnimInstance->Montage_Play(MontageToPlay);

	if (MontageLength > 0.f)
	{
		// 종료 시 Hit 태그 제거 안전망
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ABWPlayerCharacter::OnHitReactionMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, MontageToPlay);
	}
	else
	{
		// 재생 실패 — Hit 태그 즉시 해제(입력 먹통 방지)
		if (IsValid(StateComponent))
		{
			StateComponent->RemoveStateTag(BWGameplayTags::Character_State_Hit.GetTag());
		}
	}
}

void ABWPlayerCharacter::OnHitReactionMontageEnded(UAnimMontage* /*Montage*/, bool /*bInterrupted*/)
{
	// 정상 종료든 중단이든 Hit 상태를 확실히 해제한다(입력 먹통 방지 안전망).
	if (IsValid(StateComponent))
	{
		StateComponent->RemoveStateTag(BWGameplayTags::Character_State_Hit.GetTag());
	}
}

void ABWPlayerCharacter::PlayHitEffects(const FVector& ImpactPoint)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Cascade 파티클 VFX 스폰
	if (HitVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			World,
			HitVFX,
			ImpactPoint,
			FRotator::ZeroRotator,
			/*bAutoDestroy=*/true
		);
	}

	// 사운드 재생
	if (HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, HitSound, ImpactPoint);
	}
}
