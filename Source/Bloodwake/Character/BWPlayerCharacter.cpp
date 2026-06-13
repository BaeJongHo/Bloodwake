// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/BWPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Combat/BWAttributeComponent.h"
#include "Combat/BWCombatComponent.h"
#include "Combat/BWAttackComponent.h"
#include "Combat/BWTargetingComponent.h"
#include "Character/BWStateComponent.h"
#include "Core/BWGameplayDefine.h"
#include "Equipment/BWPickUpItem.h"
#include "Equipment/BWEquipItem.h"
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
}

void ABWPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// AttributeComponent 델리게이트 구독
	if (ensure(IsValid(AttributeComponent)))
	{
		AttributeComponent->OnStaminaDepleted.AddDynamic(this, &ABWPlayerCharacter::HandleStaminaDepleted);
		AttributeComponent->OnStaminaChanged.AddDynamic(this, &ABWPlayerCharacter::HandleStaminaChanged);
	}
}

void ABWPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 스태미나 소모 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SprintDrainTimerHandle);
	}

	// 델리게이트 구독 해제
	if (IsValid(AttributeComponent))
	{
		AttributeComponent->OnStaminaDepleted.RemoveDynamic(this, &ABWPlayerCharacter::HandleStaminaDepleted);
		AttributeComponent->OnStaminaChanged.RemoveDynamic(this, &ABWPlayerCharacter::HandleStaminaChanged);
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
			// Hold 전용 IA: Hold 임계 시간 충족 시 Triggered(1회) → 질주 시작.
			// 기존 Started(누르자마자) 시작에서 Triggered(Hold 충족) 시작으로 변경됨.
			// 주의: RollAction(Tap IA)과 같은 물리 키에 매핑 시 Tap은 RollAction이 담당,
			//       Hold 충족 전 Tap으로 처리되면 SprintRollingAction의 Triggered는 발생하지 않는다.
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Triggered,  this, &ABWPlayerCharacter::StartSprint);
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed,  this, &ABWPlayerCharacter::StopSprint);
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Canceled,   this, &ABWPlayerCharacter::StopSprint);
		}

		if (RollAction)
		{
			// Tap 전용 IA: 짧은 탭 충족 시 Triggered(1회) → 구르기 상태 진입.
			EnhancedInput->BindAction(RollAction, ETriggerEvent::Triggered, this, &ABWPlayerCharacter::Roll);
		}

		if (InteractAction)
		{
			// 누르는 순간 1회(Started) → 구체 스윕으로 픽업 감지 후 장착 위임.
			EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::Interact);
		}

		if (ToggleWeaponAction)
		{
			// 누르는 순간 1회(Started) → 무기 손↔등 토글.
			EnhancedInput->BindAction(ToggleWeaponAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::ToggleWeapon);
		}

		if (ToggleShieldAction)
		{
			// 누르는 순간 1회(Started) → 방패 손↔등 토글.
			EnhancedInput->BindAction(ToggleShieldAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::ToggleShield);
		}

		if (AttackAction)
		{
			// IA_Attack: Started(누르는 순간) → PrimaryTap(Light/Running 분기), Triggered(Hold 충족) → PrimaryHold(Special).
			EnhancedInput->BindAction(AttackAction, ETriggerEvent::Canceled, this, &ABWPlayerCharacter::OnAttackStarted);
			EnhancedInput->BindAction(AttackAction, ETriggerEvent::Triggered, this, &ABWPlayerCharacter::OnAttackHold);
		}

		if (HeavyAttackAction)
		{
			// IA_HeavyAttack: Started(누르는 순간) → Heavy.
			EnhancedInput->BindAction(HeavyAttackAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::OnHeavyAttack);
		}

		if (LockOnAction)
		{
			// IA_LockOn: Started(누르는 순간) → 락온 토글.
			EnhancedInput->BindAction(LockOnAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::OnLockOn);
		}

		if (SwitchTargetAction)
		{
			// IA_SwitchTarget: Started(입력이 문턱을 넘는 순간 1회) → 타깃 전환. 락온 중에만 유효.
			// Triggered가 아닌 Started를 쓰는 이유: 마우스 플릭/스틱/키 홀드 모두 "한 제스처 = 한 칸 전환"으로 만들기 위함.
			EnhancedInput->BindAction(SwitchTargetAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::OnSwitchTarget);
		}
	}
}

void ABWPlayerCharacter::Move(const FInputActionValue& Value)
{
	// 구르기 또는 공격 중에는 이동 입력을 무시한다.
	if (IsRolling() || IsAttacking())
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
	// 구르기 또는 공격 중에는 점프를 차단한다.
	if (IsRolling() || IsAttacking())
	{
		return;
	}

	Jump();
}

bool ABWPlayerCharacter::IsRolling() const
{
	return IsValid(StateComponent) && StateComponent->HasStateTag(BWGameplayTags::Character_State_Roll.GetTag());
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

	// 이미 구르는 중이거나 공격 중이면 중복 입력 무시.
	if (IsRolling() || IsAttacking())
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

	// TODO(후속): i-frame 활성화.

	// 구르기 상태 진입(Normal 자동 해제). 이 동안 Move/Jump가 차단된다.
	StateComponent->AddStateTag(BWGameplayTags::Character_State_Roll.GetTag());

	// 스태미나 소비. ConsumeStamina가 소비 시점(LastConsumeTime)을 기록하고 regen 타이머를 재가동하므로,
	// AttributeComponent의 RegenDelay(1초) 경과 후 스태미나가 자동으로 다시 회복된다.
	if (IsValid(AttributeComponent))
	{
		AttributeComponent->ConsumeStamina(RollStaminaCost);
	}

	// 회피 몽타주 재생.
	// 1) 정상 흐름: 몽타주에 배치한 커스텀 AnimNotify("RollEnd")가 회복 시점에 EndRoll을 호출(이른 입력 허용).
	// 2) 안전망: 몽타주 종료 델리게이트를 바인딩해, 공격 등 다른 몽타주가 구르기를 끊어
	//    RollEnd 노티파이가 불리지 못한 경우(bInterrupted=true)에도 EndRoll을 보장 호출한다.
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
	// EndRoll은 태그가 이미 없으면 무시되므로 중복 호출이 안전하다.
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

	// TargetingComponent 내부의 StartLockOn/EndLockOn에서 SetIsLockedOnCached를 호출해 동기화된다.
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

	// Axis1D 값 추출 (좌: 음수, 우: 양수)
	const float AxisX = Value.Get<float>();
	TargetingComponent->SwitchTargetWithDirection(AxisX);
}

bool ABWPlayerCharacter::CanSprint() const
{
	if (!IsValid(AttributeComponent))
	{
		return false;
	}

	// 최소 한 틱분 스태미나를 보유해야 질주 진입 가능.
	// 이보다 적으면 BeginSprinting 직후 첫 TickSprintDrain에서 실패해 즉시 종료되는 진입/종료 반복이 발생한다.
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

	// 픽업 Transform 캡처 — EquipNewItem 내부 교체 드롭 시 이 위치에 기존 장비를 드롭한다.
	// 1회 트레이스 후 동기 처리이므로, 드롭으로 생성된 새 픽업은 이 호출 내에서 재감지되지 않는다(무한 루프 방지).
	const FTransform BestTransform = Best->GetActorTransform();

	// 장착을 CombatComponent에 위임하고, 성공 시에만 원본 픽업을 소비한다.
	// EquipNewItem 내부에서 교체 드롭(DropEquipItem)이 완료된 후 반환되므로 순서 보장됨.
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
	if (!IsValid(AttackComponent))
	{
		return;
	}

	AttackComponent->RequestAttack(EBWAttackInputKind::PrimaryTap);
}

void ABWPlayerCharacter::OnAttackHold(const FInputActionValue& /*Value*/)
{
	if (!IsValid(AttackComponent))
	{
		return;
	}

	AttackComponent->RequestAttack(EBWAttackInputKind::PrimaryHold);
}

void ABWPlayerCharacter::OnHeavyAttack(const FInputActionValue& /*Value*/)
{
	if (!IsValid(AttackComponent))
	{
		return;
	}

	AttackComponent->RequestAttack(EBWAttackInputKind::Heavy);
}
