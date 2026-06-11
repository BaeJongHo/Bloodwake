// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWCombatComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Equipment/BWEquipItem.h"
#include "Equipment/BWPickUpItem.h"
#include "Character/BWStateComponent.h"
#include "Core/BWGameplayDefine.h"

UBWCombatComponent::UBWCombatComponent()
{
	// 장비 상태 변경은 이벤트 기반으로 처리한다. 매 프레임 틱 비활성.
	PrimaryComponentTick.bCanEverTick = false;
}

void UBWCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	// 소유 캐릭터의 메시, StateComponent, ACharacter를 1회 캐시한다.
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		CachedOwnerCharacter = OwnerCharacter;
		CachedOwnerMesh = OwnerCharacter->GetMesh();
		CachedStateComponent = OwnerCharacter->FindComponentByClass<UBWStateComponent>();
	}
	else
	{
		AActor* Owner = GetOwner();
		UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] 소유자가 ACharacter가 아닙니다. (%s)"), Owner ? *Owner->GetName() : TEXT("nullptr"));
	}

	// DefaultEquipItemClass가 지정된 경우 BeginPlay에서 자동 장착한다.
	// 드롭 Transform은 미사용이므로 Identity로 전달한다.
	if (DefaultEquipItemClass)
	{
		EquipNewItem(DefaultEquipItemClass, FTransform::Identity);
	}
}

bool UBWCombatComponent::EquipNewItem(TSubclassOf<ABWEquipItem> InEquipItemClass, const FTransform& DropTransform)
{
	// 1. null 가드
	if (!InEquipItemClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] EquipNewItem: EquipItemClass가 null입니다."));
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 2. CDO에서 슬롯 판정. None이면 베이스 클래스 직접 스폰 방지를 위해 거부.
	const ABWEquipItem* CDO = InEquipItemClass.GetDefaultObject();
	if (!CDO)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] EquipNewItem: CDO를 얻지 못했습니다. (%s)"), *InEquipItemClass->GetName());
		return false;
	}

	const EBWEquipSlot Slot = CDO->GetEquipSlot();
	if (Slot == EBWEquipSlot::None)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] EquipNewItem: 슬롯이 None입니다. ABWEquipItem 베이스 또는 미설정 파생 클래스는 직접 장착할 수 없습니다. (%s)"), *InEquipItemClass->GetName());
		return false;
	}

	// 3. 해당 슬롯에 기존 장비가 있으면 드롭 후 슬롯 초기화.
	TObjectPtr<ABWEquipItem>& SlotItemRef = GetSlotItemRef(Slot);
	bool& SlotDrawnRef = GetSlotDrawnRef(Slot);

	if (IsValid(SlotItemRef))
	{
		UE_LOG(LogTemp, Log, TEXT("[BWCombatComponent] 슬롯(%s)에 기존 장비(%s)가 있어 드롭 후 교체합니다."),
			Slot == EBWEquipSlot::Weapon ? TEXT("Weapon") : TEXT("Shield"),
			*SlotItemRef->GetName());

		DropEquipItem(SlotItemRef.Get(), DropTransform);
		SlotItemRef = nullptr;
		SlotDrawnRef = false;
	}

	// 4. 새 장비 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform SpawnTransform = GetOwner()
		? GetOwner()->GetActorTransform()
		: FTransform::Identity;

	ABWEquipItem* NewItem = World->SpawnActor<ABWEquipItem>(InEquipItemClass, SpawnTransform, SpawnParams);
	if (!IsValid(NewItem))
	{
		// B-1: 교체 흐름에서 기존 장비 드롭 후 새 장비 스폰이 실패한 경우 슬롯이 비어 비원자성 상태가 된다.
		// 원본 픽업은 월드에 그대로 남아 있으므로, 플레이어가 다시 주울 수 있다.
		UE_LOG(LogTemp, Warning,
			TEXT("[BWCombatComponent] EquipNewItem: 기존 장비를 드롭했으나 새 장비(%s) 스폰에 실패해 슬롯이 비었습니다. 원본 픽업은 월드에 남아 있습니다."),
			*InEquipItemClass->GetName());
		return false;
	}

	// 스폰 직후 콜리전·물리를 비활성화한다(캐릭터와 충돌 방지).
	NewItem->SetEquippedPhysicsState();

	// 슬롯 포인터 저장, drawn 플래그는 false(등 보관부터 시작).
	SlotItemRef = NewItem;
	SlotDrawnRef = false;

	// 5. 새 장비를 등(보관) 소켓에 초기 부착.
	const FName BackSocket = GetBackSocketForSlot(Slot);
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (OwnerMesh && ValidateSocket(BackSocket))
	{
		NewItem->MoveToHolster(OwnerMesh, BackSocket);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] 등 소켓(%s)이 유효하지 않아 초기 부착을 생략합니다."), *BackSocket.ToString());
	}

	UE_LOG(LogTemp, Log, TEXT("[BWCombatComponent] 장비 장착 완료(슬롯=%s): %s"),
		Slot == EBWEquipSlot::Weapon ? TEXT("Weapon") : TEXT("Shield"),
		*NewItem->GetName());

	return true;
}

void UBWCombatComponent::ToggleWeapon()
{
	if (!IsValid(EquippedWeapon))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] ToggleWeapon: 보유 무기가 없습니다."));
		return;
	}

	// 이미 장착/해제 모션이 진행 중이면 중복 입력을 무시한다.
	if (IsEquipActionInProgress())
	{
		return;
	}

	PlayEquipMontage(EBWEquipSlot::Weapon, /*bDraw=*/!bWeaponDrawn);
}

void UBWCombatComponent::ToggleShield()
{
	if (!IsValid(EquippedShield))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] ToggleShield: 보유 방패가 없습니다."));
		return;
	}

	// 이미 장착/해제 모션이 진행 중이면 중복 입력을 무시한다.
	if (IsEquipActionInProgress())
	{
		return;
	}

	PlayEquipMontage(EBWEquipSlot::Shield, /*bDraw=*/!bShieldDrawn);
}

void UBWCombatComponent::AttachSlotToSocket(EBWEquipSlot Slot, EBWAttachDestination Destination)
{
	if (Destination == EBWAttachDestination::Hand)
	{
		AttachSlotToHand(Slot);
	}
	else
	{
		AttachSlotToBack(Slot);
	}
}

// ── private 헬퍼 ─────────────────────────────────────────────────────────────

USkeletalMeshComponent* UBWCombatComponent::GetOwnerMesh() const
{
	if (CachedOwnerMesh.IsValid())
	{
		return CachedOwnerMesh.Get();
	}

	UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] GetOwnerMesh: 캐시된 메시가 유효하지 않습니다."));
	return nullptr;
}

bool UBWCombatComponent::ValidateSocket(const FName& SocketName) const
{
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!OwnerMesh)
	{
		return false;
	}

	if (!OwnerMesh->DoesSocketExist(SocketName))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWCombatComponent] 소켓 '%s'이 스켈레톤에 존재하지 않습니다. 스켈레톤에 소켓을 추가하거나 BP에서 올바른 소켓명을 지정하세요."),
			*SocketName.ToString());
		return false;
	}

	return true;
}

FName UBWCombatComponent::GetHandSocketForSlot(EBWEquipSlot Slot) const
{
	switch (Slot)
	{
	case EBWEquipSlot::Weapon: return WeaponHandSocketName;
	case EBWEquipSlot::Shield: return ShieldHandSocketName;
	default:                   return NAME_None;
	}
}

FName UBWCombatComponent::GetBackSocketForSlot(EBWEquipSlot Slot) const
{
	switch (Slot)
	{
	case EBWEquipSlot::Weapon: return WeaponBackSocketName;
	case EBWEquipSlot::Shield: return ShieldBackSocketName;
	default:                   return NAME_None;
	}
}

TObjectPtr<ABWEquipItem>& UBWCombatComponent::GetSlotItemRef(EBWEquipSlot Slot)
{
	ensureMsgf(Slot != EBWEquipSlot::None, TEXT("[BWCombatComponent] None 슬롯은 슬롯 참조를 반환할 수 없습니다."));

	if (Slot == EBWEquipSlot::Shield)
	{
		return EquippedShield;
	}
	// Weapon이거나 None(호출 전 검증 필수)
	return EquippedWeapon;
}

bool& UBWCombatComponent::GetSlotDrawnRef(EBWEquipSlot Slot)
{
	ensureMsgf(Slot != EBWEquipSlot::None, TEXT("[BWCombatComponent] None 슬롯은 슬롯 참조를 반환할 수 없습니다."));

	if (Slot == EBWEquipSlot::Shield)
	{
		return bShieldDrawn;
	}
	// Weapon이거나 None(호출 전 검증 필수)
	return bWeaponDrawn;
}

void UBWCombatComponent::DropEquipItem(ABWEquipItem* ItemToDrop, const FTransform& DropTransform)
{
	if (!IsValid(ItemToDrop))
	{
		return;
	}

	// 클래스 캡처 — Destroy 전에 반드시 수행.
	TSubclassOf<ABWEquipItem> DropClass = ItemToDrop->GetClass();

	if (!DropPickUpClass)
	{
		// 드롭 픽업 클래스 미설정 — 기존 장비만 파괴하고 픽업 미생성(안전 폴백).
		UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] DropEquipItem: DropPickUpClass가 설정되지 않았습니다. 기존 장비(%s)만 파괴합니다."), *ItemToDrop->GetName());
		ItemToDrop->Destroy();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		ItemToDrop->Destroy();
		return;
	}

	// 드롭 위치 계산: 현재 캐릭터 위치 기준 + 보정 오프셋.
	// (DropTransform은 소유자가 없을 때만 폴백으로 사용한다.)
	AActor* OwnerActor = GetOwner();
	FVector DropLocation = (OwnerActor ? OwnerActor->GetActorLocation() : DropTransform.GetLocation()) + DropLocationOffset;

	// 캐릭터 위치는 캡슐 중심(발보다 위)이라 그대로 두면 픽업이 공중에 뜬다.
	// 하향 라인 트레이스로 바닥을 찾아 지면에 안착시킨다(물리 시뮬레이션 없이 안정적).
	if (DropGroundTraceDistance > 0.f)
	{
		const FVector TraceStart = DropLocation;
		const FVector TraceEnd = DropLocation - FVector(0.f, 0.f, DropGroundTraceDistance);
		FHitResult GroundHit;
		FCollisionQueryParams GroundParams(TEXT("BWDropGroundTrace"), /*bTraceComplex=*/false, OwnerActor);
		if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, GroundParams))
		{
			DropLocation = GroundHit.ImpactPoint;
		}
	}

	// DropHeightOffset은 지면 안착 후 표식 메시가 바닥에 묻히지 않도록 위로 띄우는 보정값.
	DropLocation.Z += DropHeightOffset;
	const FRotator DropRotation = OwnerActor ? OwnerActor->GetActorRotation() : DropTransform.GetRotation().Rotator();
	const FTransform DropFinalTransform(DropRotation, DropLocation, FVector::OneVector);

	// SpawnActorDeferred로 생성 → EquipItemClass 주입 → FinishSpawning 순서.
	ABWPickUpItem* DroppedPickUp = World->SpawnActorDeferred<ABWPickUpItem>(
		DropPickUpClass,
		DropFinalTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (IsValid(DroppedPickUp))
	{
		DroppedPickUp->InitializeFromEquip(DropClass);
		DroppedPickUp->FinishSpawning(DropFinalTransform);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] DropEquipItem: 드롭 픽업 스폰 실패."));
	}

	// 기존 장비 파괴 (캐릭터에 부착돼 있었다면 Destroy가 detach 처리).
	ItemToDrop->Destroy();
}

void UBWCombatComponent::PlayEquipMontage(EBWEquipSlot Slot, bool bDraw)
{
	UAnimMontage* Montage = GetEquipMontage(Slot, bDraw);

	// 몽타주 미설정 시 폴백: 즉시 부착 + bool 갱신 (모션 없이도 기능은 동작)
	if (!Montage)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWCombatComponent] PlayEquipMontage: 몽타주가 설정되지 않았습니다. 즉시 부착으로 폴백합니다. (Slot=%s, bDraw=%d)"),
			Slot == EBWEquipSlot::Weapon ? TEXT("Weapon") : TEXT("Shield"), bDraw ? 1 : 0);

		if (bDraw)
		{
			AttachSlotToHand(Slot);
		}
		else
		{
			AttachSlotToBack(Slot);
		}
		// 폴백에서는 Action 태그를 걸지 않는다(즉시 완료이므로).
		return;
	}

	// AnimInstance 유효성 확인
	if (!CachedOwnerCharacter.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] PlayEquipMontage: CachedOwnerCharacter가 유효하지 않습니다."));
		return;
	}

	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!OwnerMesh)
	{
		return;
	}

	UAnimInstance* AnimInstance = OwnerMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] PlayEquipMontage: AnimInstance를 얻지 못했습니다."));
		return;
	}

	// Action 태그 부착 (장착 잠금 — 재입력 차단)
	if (CachedStateComponent.IsValid())
	{
		const FGameplayTag ActionTag = bDraw
			? BWGameplayTags::Character_Action_Equip.GetTag()
			: BWGameplayTags::Character_Action_Unequip.GetTag();
		CachedStateComponent->AddStateTag(ActionTag);
	}

	// 몽타주 재생
	const float MontageLength = AnimInstance->Montage_Play(Montage);
	if (MontageLength <= 0.f)
	{
		// 재생 실패 — Action 태그 즉시 해제하고 폴백 처리
		UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] PlayEquipMontage: Montage_Play 실패(길이=0). 즉시 부착으로 폴백합니다."));

		if (CachedStateComponent.IsValid())
		{
			CachedStateComponent->RemoveStateTag(BWGameplayTags::Character_Action_Equip.GetTag());
			CachedStateComponent->RemoveStateTag(BWGameplayTags::Character_Action_Unequip.GetTag());
		}

		if (bDraw)
		{
			AttachSlotToHand(Slot);
		}
		else
		{
			AttachSlotToBack(Slot);
		}
		return;
	}

	// 종료 델리게이트 바인딩 (Action 태그 정리 안전망)
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UBWCombatComponent::OnEquipMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
}

UAnimMontage* UBWCombatComponent::GetEquipMontage(EBWEquipSlot Slot, bool bDraw) const
{
	if (Slot == EBWEquipSlot::Weapon)
	{
		return bDraw ? EquipWeaponMontage.Get() : UnequipWeaponMontage.Get();
	}
	else if (Slot == EBWEquipSlot::Shield)
	{
		return bDraw ? EquipShieldMontage.Get() : UnequipShieldMontage.Get();
	}
	return nullptr;
}

void UBWCombatComponent::AttachSlotToHand(EBWEquipSlot Slot)
{
	TObjectPtr<ABWEquipItem>& SlotItemRef = GetSlotItemRef(Slot);
	bool& SlotDrawnRef = GetSlotDrawnRef(Slot);

	if (!IsValid(SlotItemRef))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWCombatComponent] AttachSlotToHand: 슬롯에 장비가 없습니다."));
		return;
	}

	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!OwnerMesh)
	{
		return;
	}

	const FName HandSocket = GetHandSocketForSlot(Slot);
	if (!ValidateSocket(HandSocket))
	{
		return;
	}

	SlotItemRef->MoveToHand(OwnerMesh, HandSocket);
	SlotDrawnRef = true;

	UE_LOG(LogTemp, Log, TEXT("[BWCombatComponent] AttachSlotToHand(%s): %s → 손 소켓(%s)"),
		Slot == EBWEquipSlot::Weapon ? TEXT("Weapon") : TEXT("Shield"),
		*SlotItemRef->GetName(), *HandSocket.ToString());
}

void UBWCombatComponent::AttachSlotToBack(EBWEquipSlot Slot)
{
	TObjectPtr<ABWEquipItem>& SlotItemRef = GetSlotItemRef(Slot);
	bool& SlotDrawnRef = GetSlotDrawnRef(Slot);

	if (!IsValid(SlotItemRef))
	{
		return;
	}

	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	if (!OwnerMesh)
	{
		return;
	}

	const FName BackSocket = GetBackSocketForSlot(Slot);
	if (!ValidateSocket(BackSocket))
	{
		return;
	}

	SlotItemRef->MoveToHolster(OwnerMesh, BackSocket);
	SlotDrawnRef = false;

	UE_LOG(LogTemp, Log, TEXT("[BWCombatComponent] AttachSlotToBack(%s): %s → 등 소켓(%s)"),
		Slot == EBWEquipSlot::Weapon ? TEXT("Weapon") : TEXT("Shield"),
		*SlotItemRef->GetName(), *BackSocket.ToString());
}

void UBWCombatComponent::OnEquipMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 정상 종료든 중단이든 Action 태그를 확실히 해제한다(입력 잠금 방지 안전망).
	if (CachedStateComponent.IsValid())
	{
		CachedStateComponent->RemoveStateTag(BWGameplayTags::Character_Action_Equip.GetTag());
		CachedStateComponent->RemoveStateTag(BWGameplayTags::Character_Action_Unequip.GetTag());
	}

	if (bInterrupted)
	{
		UE_LOG(LogTemp, Log, TEXT("[BWCombatComponent] OnEquipMontageEnded: 몽타주가 중단되었습니다. Action 태그 해제 완료."));
	}
}

bool UBWCombatComponent::IsEquipActionInProgress() const
{
	if (!CachedStateComponent.IsValid())
	{
		return false;
	}

	return CachedStateComponent->HasStateTag(BWGameplayTags::Character_Action_Equip.GetTag())
		|| CachedStateComponent->HasStateTag(BWGameplayTags::Character_Action_Unequip.GetTag());
}
