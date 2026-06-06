// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/BWPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Combat/BWAttributeComponent.h"

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
			// 점프는 ACharacter 내장 구현을 사용한다. 누름(Started)에 점프 시작, 뗌(Completed)에 점프 종료.
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}

		if (SprintAction)
		{
			// Hold 방식: 누르는 동안 Started, 떼면 Completed/Canceled
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &ABWPlayerCharacter::StartSprint);
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &ABWPlayerCharacter::StopSprint);
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ABWPlayerCharacter::StopSprint);
		}
	}
}

void ABWPlayerCharacter::Move(const FInputActionValue& Value)
{
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
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
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
		// 스태미나 부족 — 질주 중단(HandleStaminaDepleted가 곧 호출되므로 여기서 중복 처리 방지)
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
