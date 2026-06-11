// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWAttackComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Character/BWStateComponent.h"
#include "Combat/BWAttributeComponent.h"
#include "Core/BWGameplayDefine.h"

UBWAttackComponent::UBWAttackComponent()
{
	// 공격 로직은 입력/AnimNotify 이벤트 기반으로 처리한다. 매 프레임 틱 비활성.
	PrimaryComponentTick.bCanEverTick = false;
}

void UBWAttackComponent::BeginPlay()
{
	Super::BeginPlay();

	// 소유 캐릭터의 컴포넌트를 1회 캐시한다. BWCombatComponent BeginPlay 패턴과 동일.
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		CachedOwnerCharacter = OwnerCharacter;
		CachedOwnerMesh = OwnerCharacter->GetMesh();
		CachedStateComponent = OwnerCharacter->FindComponentByClass<UBWStateComponent>();
		CachedAttributeComponent = OwnerCharacter->FindComponentByClass<UBWAttributeComponent>();
	}
	else
	{
		const AActor* Owner = GetOwner();
		UE_LOG(LogTemp, Warning, TEXT("[BWAttackComponent] BeginPlay: 소유자가 ACharacter가 아닙니다. (%s)"),
			Owner ? *Owner->GetName() : TEXT("nullptr"));
	}
}

void UBWAttackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 진행 중 몽타주 End 델리게이트 정리 — 댕글링 콜백 방지.
	if (ActiveMontage && CachedOwnerMesh.IsValid())
	{
		if (UAnimInstance* AnimInstance = CachedOwnerMesh->GetAnimInstance())
		{
			FOnMontageEnded EmptyDelegate;
			AnimInstance->Montage_SetEndDelegate(EmptyDelegate, ActiveMontage.Get());
		}
	}

	// 입력 버퍼 클리어.
	bHasBufferedInput = false;
	ActiveMontage = nullptr;

	Super::EndPlay(EndPlayReason);
}

// ── 입력 진입점 ───────────────────────────────────────────────────────────────

void UBWAttackComponent::RequestAttack(EBWAttackInputKind InputKind)
{
	// 1. 입력 종류 + 현재 상태로 EBWAttackType 결정.
	EBWAttackType ResolvedType = EBWAttackType::Light;

	if (InputKind == EBWAttackInputKind::PrimaryHold)
	{
		// Light 극초기 캔슬 → Special로 전환.
		// 진행 중 몽타주를 중단하고 Special을 처음부터 시작한다.
		if (CachedOwnerMesh.IsValid())
		{
			if (UAnimInstance* AnimInstance = CachedOwnerMesh->GetAnimInstance())
			{
				if (ActiveMontage)
				{
					AnimInstance->Montage_Stop(0.1f, ActiveMontage.Get());
				}
			}
		}
		// Attack 태그 해제(StartAttack이 새 태그를 붙이기 전 클린 상태 보장).
		// CurrentAttackType이 Light일 때만 Special 전환을 허용하므로, 해제 태그도 CurrentAttackType(=Light)을 사용한다.
		// Running 등 다른 타입이 진행 중일 때는 이 분기 자체에 진입하지 않는다(아래 조건 참조).
		if (CachedStateComponent.IsValid())
		{
			CachedStateComponent->RemoveStateTag(GetTagForAttackType(CurrentAttackType));
		}
		CurrentComboIndex = 0;
		bComboWindowOpen = false;
		bHasBufferedInput = false;
		StartAttack(EBWAttackType::Special, 0);
		return;
	}
	else if (InputKind == EBWAttackInputKind::Heavy)
	{
		ResolvedType = EBWAttackType::Heavy;
	}
	else // PrimaryTap
	{
		// Sprint 태그 보유 중이면 Running.
		if (CachedStateComponent.IsValid()
			&& CachedStateComponent->HasStateTag(BWGameplayTags::Character_State_Sprint.GetTag()))
		{
			ResolvedType = EBWAttackType::Running;
		}
		else
		{
			ResolvedType = EBWAttackType::Light;
		}

		// Tap 시각 기록 (Hold GraceTime 판정용).
		LastPrimaryTapTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	}

	// 2. 발동 흐름 결정.
	if (!IsAttacking())
	{
		// 2a. 공격 중 아님 → 콤보 첫 단계 시작.
		CurrentComboIndex = 0;
		StartAttack(ResolvedType, 0);
	}
	else if (bComboWindowOpen)
	{
		// 2b. 공격 중 + 콤보 윈도우 열림.
		// 콤보형(Light/Heavy)이고 현재 진행 중 타입과 동일할 때만 다음 단계로 진행한다.
		// - Running/Special은 단발형 — 콤보 진행 없음, 무시.
		// - 타입 불일치(예: Light 콤보 중 Heavy 입력): 현재 콤보를 잇지 않고 무시한다.
		//   설계 문서에 타입 전환 처리 명시 없으므로, 가장 단순한 정책으로 무시를 채택한다.
		//   (버퍼 저장도 하지 않음 — 다른 타입을 버퍼에 넣으면 윈도우 닫힌 후 의도치 않은 타입이 소비됨)
		const bool bIsComboType = (ResolvedType == EBWAttackType::Light || ResolvedType == EBWAttackType::Heavy);
		if (bIsComboType && ResolvedType == CurrentAttackType)
		{
			const FBWAttackComboRow* Row = FindRow(CurrentAttackType);
			if (Row && Row->Steps.Num() > 0)
			{
				const int32 NextIndex = (CurrentComboIndex + 1) % Row->Steps.Num();
				StartAttack(CurrentAttackType, NextIndex);
			}
		}
	}
	else
	{
		// 2c. 공격 중 + 윈도우 닫힘 → 버퍼에 저장(가장 마지막 입력 1개만).
		BufferedInputKind = InputKind;
		bHasBufferedInput = true;
	}
}

bool UBWAttackComponent::IsAttacking() const
{
	if (!CachedStateComponent.IsValid())
	{
		return false;
	}

	// 4개 Attack 태그 중 하나라도 보유하면 공격 중.
	return CachedStateComponent->HasStateTag(BWGameplayTags::Character_Attack_Light.GetTag())
		|| CachedStateComponent->HasStateTag(BWGameplayTags::Character_Attack_Running.GetTag())
		|| CachedStateComponent->HasStateTag(BWGameplayTags::Character_Attack_Special.GetTag())
		|| CachedStateComponent->HasStateTag(BWGameplayTags::Character_Attack_Heavy.GetTag());
}

// ── AnimNotifyState / AnimNotify 진입점 ──────────────────────────────────────

void UBWAttackComponent::OpenComboWindow()
{
	bComboWindowOpen = true;

	// 버퍼된 입력이 있으면 즉시 소비(선입력 허용 — 소울라이크 손맛).
	if (bHasBufferedInput)
	{
		bHasBufferedInput = false;
		const EBWAttackInputKind Buffered = BufferedInputKind;
		RequestAttack(Buffered);
	}
}

void UBWAttackComponent::CloseComboWindow()
{
	bComboWindowOpen = false;
	// 미소비 버퍼 폐기 — 윈도우가 닫힌 후 입력은 버퍼에 새로 쌓이지 않으면 다음 공격 첫 단계부터 시작.
	bHasBufferedInput = false;
}

void UBWAttackComponent::NotifyComboEnd()
{
	// 콤보 인덱스 초기화 + Attack 태그 해제(StateComponent가 Normal로 자동 복귀).
	if (CachedStateComponent.IsValid())
	{
		CachedStateComponent->RemoveStateTag(GetTagForAttackType(CurrentAttackType));
	}
	CurrentComboIndex = 0;
	bComboWindowOpen = false;
	bHasBufferedInput = false;
	ActiveMontage = nullptr;
}

// ── 정적 헬퍼 ────────────────────────────────────────────────────────────────

UBWAttackComponent* UBWAttackComponent::FindAttackComponent(const USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWAttackComponent] FindAttackComponent: MeshComp가 null입니다."));
		return nullptr;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWAttackComponent] FindAttackComponent: MeshComp 소유 액터가 유효하지 않습니다."));
		return nullptr;
	}

	UBWAttackComponent* AttackComp = Owner->FindComponentByClass<UBWAttackComponent>();
	if (!AttackComp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWAttackComponent] FindAttackComponent: 소유 액터(%s)에서 UBWAttackComponent를 찾지 못했습니다."),
			*Owner->GetName());
	}

	return AttackComp;
}

// ── private 구현부 ────────────────────────────────────────────────────────────

void UBWAttackComponent::StartAttack(EBWAttackType Type, int32 StepIndex)
{
	// 1. DataTable Row 조회.
	const FBWAttackComboRow* Row = FindRow(Type);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWAttackComponent] StartAttack: DataTable에서 Row(%s)를 찾지 못했습니다. AttackDataTable이 설정됐는지, Row 키가 맞는지 확인하세요."),
			*GetRowNameForAttackType(Type).ToString());
		return;
	}

	// 2. Steps 경계 검사.
	if (Row->Steps.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWAttackComponent] StartAttack: Row(%s)의 Steps가 비어 있습니다. DataTable에 몽타주를 추가하세요."),
			*GetRowNameForAttackType(Type).ToString());
		return;
	}

	const int32 ClampedIndex = StepIndex % Row->Steps.Num();
	const FBWComboStep& Step = Row->Steps[ClampedIndex];

	// 3. 몽타주 유효성 검사.
	if (!Step.Montage)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWAttackComponent] StartAttack: Steps[%d].Montage가 null입니다. DataTable에서 몽타주를 지정하세요."),
			ClampedIndex);
		return;
	}

	// 4. 스태미나 체크.
	if (!CachedAttributeComponent.IsValid() || !CachedAttributeComponent->HasEnoughStamina(Step.StaminaCost))
	{
		// 스태미나 부족 — 태그·몽타주 미시작(상태 갇힘 방지).
		return;
	}

	// 5. AnimInstance 유효성 확인.
	if (!CachedOwnerMesh.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWAttackComponent] StartAttack: CachedOwnerMesh가 유효하지 않습니다."));
		return;
	}

	UAnimInstance* AnimInstance = CachedOwnerMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWAttackComponent] StartAttack: AnimInstance를 얻지 못했습니다."));
		return;
	}

	// 6. 이전 Attack 태그 해제(타입 변경 또는 같은 타입 콤보 진행 시).
	if (CachedStateComponent.IsValid() && IsAttacking())
	{
		CachedStateComponent->RemoveStateTag(GetTagForAttackType(CurrentAttackType));
	}

	// 7. 새 Attack 상태 태그 부착.
	CurrentAttackType = Type;
	CurrentComboIndex = ClampedIndex;
	bComboWindowOpen = false;

	if (CachedStateComponent.IsValid())
	{
		CachedStateComponent->AddStateTag(GetTagForAttackType(Type));
	}

	// 8. ActiveMontage를 Montage_Play 호출 전에 설정한다.
	// → 이전 몽타주 Stop으로 인한 EndDelegate 콜백이 오더라도 새 ActiveMontage와 다른 몽타주이므로
	//   OnAttackMontageEnded 가드(Montage != ActiveMontage)가 이를 무시한다(수정 3 참조).
	ActiveMontage = Step.Montage;

	// 9. 몽타주 재생.
	const float MontageLength = AnimInstance->Montage_Play(Step.Montage.Get());

	if (MontageLength <= 0.f)
	{
		// 재생 실패 — Attack 태그 즉시 해제(상태 갇힘/입력 먹통 방지).
		// 스태미나는 재생 성공 확인 전이므로 소비하지 않는다(스태미나 누수 방지).
		UE_LOG(LogTemp, Warning,
			TEXT("[BWAttackComponent] StartAttack: Montage_Play 실패(길이=0). Attack 태그를 즉시 해제합니다."));

		if (CachedStateComponent.IsValid())
		{
			CachedStateComponent->RemoveStateTag(GetTagForAttackType(Type));
		}
		CurrentComboIndex = 0;
		ActiveMontage = nullptr;
		return;
	}

	// 10. 스태미나 소비 — 몽타주 재생 성공 확인 후에 소비한다(재생 실패 시 누수 방지).
	CachedAttributeComponent->ConsumeStamina(Step.StaminaCost);

	// 11. 종료 델리게이트 바인딩(ComboEnd 노티파이 미배치/미도달 안전망).
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UBWAttackComponent::OnAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Step.Montage.Get());
}

void UBWAttackComponent::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 동일성 가드: 콤보 진행 시 StartAttack이 이전 몽타주를 Stop하면 이전 몽타주의 EndDelegate가 호출된다.
	// ActiveMontage는 StartAttack에서 Montage_Play 전에 새 몽타주로 갱신되므로,
	// 교체된 이전 몽타주의 콜백은 여기서 무시해 CurrentComboIndex 초기화·Attack 태그 해제를 막는다.
	if (Montage != ActiveMontage)
	{
		return;
	}

	// 정상 종료든 중단이든 Attack 상태를 확실히 해제한다(입력 먹통 방지 안전망).
	// NotifyComboEnd가 이미 호출됐으면 태그가 없어 RemoveStateTag가 무시되므로 중복 호출 안전.
	if (CachedStateComponent.IsValid())
	{
		CachedStateComponent->RemoveStateTag(GetTagForAttackType(CurrentAttackType));
	}
	CurrentComboIndex = 0;
	bComboWindowOpen = false;
	bHasBufferedInput = false;
	ActiveMontage = nullptr;

	if (bInterrupted)
	{
		UE_LOG(LogTemp, Log, TEXT("[BWAttackComponent] OnAttackMontageEnded: 몽타주가 중단되었습니다. Attack 태그 해제 완료."));
	}
}

const FBWAttackComboRow* UBWAttackComponent::FindRow(EBWAttackType Type) const
{
	if (!AttackDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWAttackComponent] FindRow: AttackDataTable이 설정되지 않았습니다. BP 자식에서 DT_PlayerAttacks를 지정하세요."));
		return nullptr;
	}

	const FName RowName = GetRowNameForAttackType(Type);
	const FBWAttackComboRow* Row = AttackDataTable->FindRow<FBWAttackComboRow>(RowName, TEXT("BWAttackComponent::FindRow"));
	if (!Row)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWAttackComponent] FindRow: RowName('%s')에 해당하는 Row가 없습니다. DataTable에 행을 추가했는지 확인하세요."),
			*RowName.ToString());
	}

	return Row;
}

FGameplayTag UBWAttackComponent::GetTagForAttackType(EBWAttackType Type) const
{
	switch (Type)
	{
	case EBWAttackType::Light:   return BWGameplayTags::Character_Attack_Light.GetTag();
	case EBWAttackType::Running: return BWGameplayTags::Character_Attack_Running.GetTag();
	case EBWAttackType::Special: return BWGameplayTags::Character_Attack_Special.GetTag();
	case EBWAttackType::Heavy:   return BWGameplayTags::Character_Attack_Heavy.GetTag();
	default:
		UE_LOG(LogTemp, Warning, TEXT("[BWAttackComponent] GetTagForAttackType: 알 수 없는 공격 타입입니다."));
		return FGameplayTag::EmptyTag;
	}
}

FName UBWAttackComponent::GetRowNameForAttackType(EBWAttackType Type) const
{
	switch (Type)
	{
	case EBWAttackType::Light:   return FName(TEXT("Light"));
	case EBWAttackType::Running: return FName(TEXT("Running"));
	case EBWAttackType::Special: return FName(TEXT("Special"));
	case EBWAttackType::Heavy:   return FName(TEXT("Heavy"));
	default:
		UE_LOG(LogTemp, Warning, TEXT("[BWAttackComponent] GetRowNameForAttackType: 알 수 없는 공격 타입입니다."));
		return NAME_None;
	}
}
