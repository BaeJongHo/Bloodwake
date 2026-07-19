// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWCombatComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/DataTable.h"
#include "Equipment/BWEquipItem.h"
#include "Equipment/BWWeapon.h"
#include "Equipment/BWArmour.h"
#include "Equipment/BWPickUpItem.h"
#include "Character/BWStateComponent.h"
#include "Character/BWPlayerCharacter.h"
#include "Combat/BWAttackComponent.h"
#include "Combat/BWAttackTypes.h"
#include "Combat/BWWeaponCollisionComponent.h"
#include "Combat/BWAttributeComponent.h"
#include "Core/BWGameplayDefine.h"

UBWCombatComponent::UBWCombatComponent()
{
	// 장비 상태 변경은 이벤트 기반으로 처리한다. 매 프레임 틱 비활성.
	PrimaryComponentTick.bCanEverTick = false;

	// Main/Second 히트 콜리전 컴포넌트를 이 컴포넌트의 소유로 생성한다.
	// 실제 sweep 소스는 BeginPlay 이후 RefreshCombatState에서 주입된다.
	MainCollision = CreateDefaultSubobject<UBWWeaponCollisionComponent>(TEXT("MainCollision"));
	SecondCollision = CreateDefaultSubobject<UBWWeaponCollisionComponent>(TEXT("SecondCollision"));
}

void UBWCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	// 소유 캐릭터의 메시, StateComponent, AttackComponent, ACharacter, AttributeComponent를 1회 캐시한다.
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		CachedOwnerCharacter = OwnerCharacter;
		CachedOwnerMesh = OwnerCharacter->GetMesh();
		CachedStateComponent = OwnerCharacter->FindComponentByClass<UBWStateComponent>();
		CachedAttackComponent = OwnerCharacter->FindComponentByClass<UBWAttackComponent>();
		CachedAttributeComponent = OwnerCharacter->FindComponentByClass<UBWAttributeComponent>();
	}
	else
	{
		const AActor* Owner = GetOwner();
		UE_LOG(LogTemp, Warning,
			TEXT("[BWCombatComponent] 소유자가 ACharacter가 아닙니다. (%s)"),
			Owner ? *Owner->GetName() : TEXT("nullptr"));
	}

	// DefaultEquipItemClass가 지정된 경우 BeginPlay에서 자동 장착한다.
	// 드롭 Transform은 미사용이므로 Identity로 전달한다.
	if (DefaultEquipItemClass)
	{
		EquipNewItem(DefaultEquipItemClass, FTransform::Identity);
	}

	// 기본 장비 장착 완료 후 장비 상태를 브로드캐스트한다.
	// HUD는 다음 틱(GameMode::InitializeHUD)에 구독하므로 이 시점엔 수신자가 없지만,
	// HUD가 InitializeForPlayer에서 BroadcastEquipmentState를 재호출해 초기 동기화를 보장한다.
	BroadcastEquipmentState();

	// 초기 전투 상태 설정 — 무기는 항상 등부터 시작하므로 초기엔 MeleeFists.
	RefreshCombatState();
}

void UBWCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 진행 중 Equip 몽타주 End 델리게이트 정리 — 댕글링 콜백 방지(BWAttackComponent::EndPlay 패턴 동일).
	if (ActiveEquipMontage && CachedOwnerMesh.IsValid())
	{
		if (UAnimInstance* AnimInstance = CachedOwnerMesh->GetAnimInstance())
		{
			FOnMontageEnded EmptyDelegate;
			AnimInstance->Montage_SetEndDelegate(EmptyDelegate, ActiveEquipMontage.Get());
		}
	}

	ActiveEquipMontage = nullptr;

	Super::EndPlay(EndPlayReason);
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
		UE_LOG(LogTemp, Warning,
			TEXT("[BWCombatComponent] EquipNewItem: CDO를 얻지 못했습니다. (%s)"),
			*InEquipItemClass->GetName());
		return false;
	}

	const EBWEquipSlot Slot = CDO->GetEquipSlot();
	if (Slot == EBWEquipSlot::None)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWCombatComponent] EquipNewItem: 슬롯이 None입니다. ABWEquipItem 베이스 또는 미설정 파생 클래스는 직접 장착할 수 없습니다. (%s)"),
			*InEquipItemClass->GetName());
		return false;
	}

	// Armour 슬롯은 별도 경로(TMap 관리)로 처리한다 — Weapon/Shield 단일 참조 패턴과 다름.
	if (Slot == EBWEquipSlot::Armour)
	{
		EquipArmourItem(InEquipItemClass, DropTransform);
		return true;
	}

	// 3. 해당 슬롯에 기존 장비가 있으면 드롭 후 슬롯 초기화.
	TObjectPtr<ABWEquipItem>& SlotItemRef = GetSlotItemRef(Slot);
	bool& SlotDrawnRef = GetSlotDrawnRef(Slot);

	if (IsValid(SlotItemRef))
	{
		UE_LOG(LogTemp, Log,
			TEXT("[BWCombatComponent] 슬롯(%s)에 기존 장비(%s)가 있어 드롭 후 교체합니다."),
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
		UE_LOG(LogTemp, Warning,
			TEXT("[BWCombatComponent] 등 소켓(%s)이 유효하지 않아 초기 부착을 생략합니다."),
			*BackSocket.ToString());
	}

	UE_LOG(LogTemp, Log,
		TEXT("[BWCombatComponent] 장비 장착 완료(슬롯=%s): %s"),
		Slot == EBWEquipSlot::Weapon ? TEXT("Weapon") : TEXT("Shield"),
		*NewItem->GetName());

	// 장비 교체 완료 — HUD 장비 위젯에 변경을 알린다.
	OnEquipmentChanged.Broadcast(Slot, SlotItemRef.Get());

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

	// 콤보 공격 중 무기 토글을 차단한다 — 활성 DataTable 스왑으로 인한 인덱스 불일치 방지.
	if (CachedAttackComponent.IsValid() && CachedAttackComponent->IsAttacking())
	{
		UE_LOG(LogTemp, Log,
			TEXT("[BWCombatComponent] ToggleWeapon: 공격 중이므로 무기 토글을 무시합니다."));
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

	// 양손 무기가 손에 있을 경우 방패 토글을 차단한다.
	if (bWeaponDrawn)
	{
		if (const ABWWeapon* Weapon = Cast<ABWWeapon>(EquippedWeapon))
		{
			if (Weapon->GetCombatType() == EBWCombatType::TwoHanded)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[BWCombatComponent] ToggleShield: 양손 무기가 손에 있어 방패 토글을 차단합니다."));
				return;
			}
		}
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

UBWWeaponCollisionComponent* UBWCombatComponent::GetCollisionForSlot(EBWWeaponSlotSelector Slot) const
{
	return (Slot == EBWWeaponSlotSelector::Second) ? SecondCollision.Get() : MainCollision.Get();
}

void UBWCombatComponent::BroadcastEquipmentState()
{
	// Weapon · Shield 슬롯 각각 1회 브로드캐스트한다.
	// 빈 슬롯(nullptr)이면 HUD 위젯이 ClearEquipmentIcon을 호출해 빈 슬롯을 표시한다.
	OnEquipmentChanged.Broadcast(EBWEquipSlot::Weapon, EquippedWeapon.Get());
	OnEquipmentChanged.Broadcast(EBWEquipSlot::Shield, EquippedShield.Get());
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
	if (Slot == EBWEquipSlot::Weapon)
	{
		// 양손 무기 오버라이드 우선 적용
		if (const ABWWeapon* Weapon = Cast<ABWWeapon>(EquippedWeapon))
		{
			const FName Override = Weapon->GetHandSocketOverride();
			if (!Override.IsNone())
			{
				return Override;
			}
		}
		return WeaponHandSocketName;
	}

	if (Slot == EBWEquipSlot::Shield)
	{
		return ShieldHandSocketName;
	}

	return NAME_None;
}

FName UBWCombatComponent::GetBackSocketForSlot(EBWEquipSlot Slot) const
{
	if (Slot == EBWEquipSlot::Weapon)
	{
		// 양손 무기 오버라이드 우선 적용
		if (const ABWWeapon* Weapon = Cast<ABWWeapon>(EquippedWeapon))
		{
			const FName Override = Weapon->GetHolsterSocketOverride();
			if (!Override.IsNone())
			{
				return Override;
			}
		}
		return WeaponBackSocketName;
	}

	if (Slot == EBWEquipSlot::Shield)
	{
		return ShieldBackSocketName;
	}

	return NAME_None;
}

TObjectPtr<ABWEquipItem>& UBWCombatComponent::GetSlotItemRef(EBWEquipSlot Slot)
{
	ensureMsgf(Slot != EBWEquipSlot::None,
		TEXT("[BWCombatComponent] None 슬롯은 슬롯 참조를 반환할 수 없습니다."));

	if (Slot == EBWEquipSlot::Shield)
	{
		return EquippedShield;
	}
	// Weapon이거나 None(호출 전 검증 필수)
	return EquippedWeapon;
}

bool& UBWCombatComponent::GetSlotDrawnRef(EBWEquipSlot Slot)
{
	ensureMsgf(Slot != EBWEquipSlot::None,
		TEXT("[BWCombatComponent] None 슬롯은 슬롯 참조를 반환할 수 없습니다."));

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
		UE_LOG(LogTemp, Warning,
			TEXT("[BWCombatComponent] DropEquipItem: DropPickUpClass가 설정되지 않았습니다. 기존 장비(%s)만 파괴합니다."),
			*ItemToDrop->GetName());
		ItemToDrop->Destroy();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		ItemToDrop->Destroy();
		return;
	}

	AActor* OwnerActor = GetOwner();
	FVector DropLocation = (OwnerActor ? OwnerActor->GetActorLocation() : DropTransform.GetLocation()) + DropLocationOffset;

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

	DropLocation.Z += DropHeightOffset;
	const FRotator DropRotation = OwnerActor ? OwnerActor->GetActorRotation() : DropTransform.GetRotation().Rotator();
	const FTransform DropFinalTransform(DropRotation, DropLocation, FVector::OneVector);

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
		return;
	}

	if (!CachedOwnerCharacter.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWCombatComponent] PlayEquipMontage: CachedOwnerCharacter가 유효하지 않습니다."));
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
		UE_LOG(LogTemp, Warning,
			TEXT("[BWCombatComponent] PlayEquipMontage: Montage_Play 실패(길이=0). 즉시 부착으로 폴백합니다."));

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

	// 진행 중 몽타주 캐시 — EndPlay에서 델리게이트 정리에 사용한다.
	ActiveEquipMontage = Montage;

	// 종료 델리게이트 바인딩 (Action 태그 정리 안전망)
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &UBWCombatComponent::OnEquipMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
}

UAnimMontage* UBWCombatComponent::GetEquipMontage(EBWEquipSlot Slot, bool bDraw) const
{
	if (Slot == EBWEquipSlot::Weapon)
	{
		// 무기 장착/해제 몽타주는 무기 DataTable에서 조회한다(멤버 제거).
		return GetWeaponEquipMontageFromDataTable(bDraw);
	}
	else if (Slot == EBWEquipSlot::Shield)
	{
		// 방패 장착/해제 몽타주는 기존 멤버를 유지한다.
		return bDraw ? EquipShieldMontage.Get() : UnequipShieldMontage.Get();
	}
	return nullptr;
}

UAnimMontage* UBWCombatComponent::GetWeaponEquipMontageFromDataTable(bool bDraw) const
{
	const ABWWeapon* Weapon = Cast<ABWWeapon>(EquippedWeapon);
	if (!Weapon)
	{
		return nullptr;
	}

	UDataTable* DataTable = Weapon->GetAttackDataTable();
	if (!DataTable)
	{
		return nullptr;
	}

	const FName RowName = bDraw ? BWAttackRowNames::Equip : BWAttackRowNames::Unequip;
	// bWarnIfRowMissing=false: Equip/Unequip 행 미설정은 "몽타주 없음→즉시 부착 폴백" 의도된 동작이므로
	// 엔진 Warning 폭발을 억제한다.
	const FBWAttackComboRow* Row = DataTable->FindRow<FBWAttackComboRow>(RowName,
		TEXT("[BWCombatComponent] GetWeaponEquipMontageFromDataTable"), /*bWarnIfRowMissing=*/false);

	if (!Row || Row->Steps.Num() == 0)
	{
		return nullptr;
	}

	return Row->Steps[0].Montage.Get();
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

	UE_LOG(LogTemp, Log,
		TEXT("[BWCombatComponent] AttachSlotToHand(%s): %s → 손 소켓(%s)"),
		Slot == EBWEquipSlot::Weapon ? TEXT("Weapon") : TEXT("Shield"),
		*SlotItemRef->GetName(), *HandSocket.ToString());

	// Weapon 슬롯 부착 완료 시 전투 상태를 갱신한다(단일 진입점).
	if (Slot == EBWEquipSlot::Weapon)
	{
		RefreshCombatState();
	}
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

	UE_LOG(LogTemp, Log,
		TEXT("[BWCombatComponent] AttachSlotToBack(%s): %s → 등 소켓(%s)"),
		Slot == EBWEquipSlot::Weapon ? TEXT("Weapon") : TEXT("Shield"),
		*SlotItemRef->GetName(), *BackSocket.ToString());

	// Weapon 슬롯 보관 완료 시 전투 상태를 갱신한다(단일 진입점).
	if (Slot == EBWEquipSlot::Weapon)
	{
		RefreshCombatState();
	}
}

void UBWCombatComponent::OnEquipMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 정상 종료든 중단이든 Action 태그를 확실히 해제한다(입력 잠금 방지 안전망).
	if (CachedStateComponent.IsValid())
	{
		CachedStateComponent->RemoveStateTag(BWGameplayTags::Character_Action_Equip.GetTag());
		CachedStateComponent->RemoveStateTag(BWGameplayTags::Character_Action_Unequip.GetTag());
	}

	// 진행 중 몽타주 캐시 클리어.
	ActiveEquipMontage = nullptr;

	if (bInterrupted)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[BWCombatComponent] OnEquipMontageEnded: 몽타주가 중단되었습니다. Action 태그 해제 완료."));
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

// ── 방어구 슬롯 ──────────────────────────────────────────────────────────────

ABWArmour* UBWCombatComponent::GetEquippedArmour(EBWArmourType Type) const
{
	const TObjectPtr<ABWArmour>* Found = EquippedArmours.Find(Type);
	if (!Found)
	{
		return nullptr;
	}
	return Found->Get();
}

void UBWCombatComponent::EquipArmourItem(TSubclassOf<ABWEquipItem> EquipItemClass, const FTransform& DropTransform)
{
	// 1. CDO에서 ArmourType 확인 (Cast로 방어구 여부 검증)
	const ABWArmour* ArmourCDO = Cast<ABWArmour>(EquipItemClass.GetDefaultObject());
	if (!ArmourCDO)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWCombatComponent] EquipArmourItem: 클래스(%s)가 ABWArmour가 아닙니다."),
			*EquipItemClass->GetName());
		return;
	}

	const EBWArmourType ArmourType = ArmourCDO->GetArmourType();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 2. 새 방어구 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform SpawnTransform = GetOwner()
		? GetOwner()->GetActorTransform()
		: FTransform::Identity;

	ABWArmour* NewArmour = World->SpawnActor<ABWArmour>(EquipItemClass, SpawnTransform, SpawnParams);
	if (!IsValid(NewArmour))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWCombatComponent] EquipArmourItem: 새 방어구(%s) 스폰에 실패했습니다."),
			*EquipItemClass->GetName());
		return;
	}

	// 스폰 직후 콜리전·물리를 비활성화한다.
	NewArmour->SetEquippedPhysicsState();

	// 3. 같은 부위에 기존 방어구가 있으면 해제 + 드롭 후 맵 제거
	//    순서: UnequipItem(DecreaseDefense) → 맵 제거 → DropEquipItem(Destroy)
	if (TObjectPtr<ABWArmour>* ExistingPtr = EquippedArmours.Find(ArmourType))
	{
		ABWArmour* ExistingArmour = ExistingPtr->Get();
		if (IsValid(ExistingArmour))
		{
			UE_LOG(LogTemp, Log,
				TEXT("[BWCombatComponent] EquipArmourItem: 기존 방어구(%s) 해제 후 드롭합니다."),
				*ExistingArmour->GetName());

			// LeaderPose 해제 + DecreaseDefense — Destroy 전에 반드시 호출
			UBWAttributeComponent* AttrComp = CachedAttributeComponent.IsValid()
				? CachedAttributeComponent.Get() : nullptr;
			ExistingArmour->UnequipItem(AttrComp);

			// 신체 메시 복원 — UnequipItem 직후, 맵 제거 전에 호출한다.
			// 이후 신규 장착 경로에서 SetBodyPartHidden(ArmourType, true)가 다시 호출되므로 최종 상태는 올바르다.
			SetBodyPartHidden(ArmourType, false);

			// 맵에서 제거
			EquippedArmours.Remove(ArmourType);

			// 드롭 픽업 생성 후 기존 방어구 Destroy
			DropEquipItem(ExistingArmour, DropTransform);
		}
		else
		{
			EquippedArmours.Remove(ArmourType);
		}
	}

	// 4. 새 방어구 장착: 플레이어 메시에 부착 + LeaderPose + IncreaseDefense
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	UBWAttributeComponent* Attribute = CachedAttributeComponent.IsValid()
		? CachedAttributeComponent.Get() : nullptr;

	NewArmour->EquipItem(OwnerMesh, Attribute);

	// 5. 맵에 등록
	EquippedArmours.Add(ArmourType, NewArmour);

	// 6. 대응 기본 신체 메시 숨김 (Chest/Pants/Boots만; Gloves는 내부에서 무시)
	SetBodyPartHidden(ArmourType, true);

	UE_LOG(LogTemp, Log,
		TEXT("[BWCombatComponent] EquipArmourItem: %s 장착 완료 (부위=%d)"),
		*NewArmour->GetName(), static_cast<int32>(ArmourType));
}

void UBWCombatComponent::SetBodyPartHidden(EBWArmourType Type, bool bHideBodyPart)
{
	// 플레이어 캐릭터에 위임. Cast 실패(적/보스)는 조용히 무시 — 신체 파츠가 없어도 안전.
	if (ABWPlayerCharacter* PlayerCharacter = Cast<ABWPlayerCharacter>(GetOwner()))
	{
		PlayerCharacter->SetBodyArmourHidden(Type, bHideBodyPart);
	}
}

void UBWCombatComponent::RefreshCombatState()
{
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();

	if (bWeaponDrawn)
	{
		// ── 무기가 손에 있는 경우 ──────────────────────────────────────────

		const ABWWeapon* Weapon = Cast<ABWWeapon>(EquippedWeapon);
		if (!Weapon)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[BWCombatComponent] RefreshCombatState: bWeaponDrawn=true이지만 EquippedWeapon이 ABWWeapon이 아닙니다. MeleeFists로 폴백합니다."));
			// 폴백: 맨손으로 처리
			CurrentCombatType = EBWCombatType::MeleeFists;
			if (IsValid(MainCollision))  { MainCollision->ClearSource(); }
			if (IsValid(SecondCollision)) { SecondCollision->ClearSource(); }
			if (CachedAttackComponent.IsValid())
			{
				CachedAttackComponent->SetActiveAttackDataTable(FistAttackDataTable.Get());
			}
			return;
		}

		CurrentCombatType = Weapon->GetCombatType();

		// Main 콜리전: 무기 메시 소켓 sweep
		if (IsValid(MainCollision))
		{
			// 무기 SM 컴포넌트 찾기 — ABWEquipItem.MeshComponent(UStaticMeshComponent)
			UStaticMeshComponent* WeaponMesh = Weapon->FindComponentByClass<UStaticMeshComponent>();
			if (WeaponMesh)
			{
				MainCollision->ConfigureForWeapon(
					WeaponMesh,
					Weapon->GetTraceStartSocket(),
					Weapon->GetTraceEndSocket(),
					Weapon->GetTraceRadius(),
					Weapon);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[BWCombatComponent] RefreshCombatState: 무기(%s)에서 UStaticMeshComponent를 찾지 못했습니다. Main 콜리전 소스를 클리어합니다."),
					*Weapon->GetName());
				MainCollision->ClearSource();
			}
		}

		// Second 콜리전: 편손/양손 무기는 미사용
		if (IsValid(SecondCollision))
		{
			SecondCollision->ClearSource();
		}

		// AttackComponent에 무기 DataTable push
		if (CachedAttackComponent.IsValid())
		{
			CachedAttackComponent->SetActiveAttackDataTable(Weapon->GetAttackDataTable());
		}

		// 양손 무기 시 방패 강제 보관 — 무한 재진입 방지를 위해 Shield 슬롯은 RefreshCombatState를 트리거하지 않음
		if (CurrentCombatType == EBWCombatType::TwoHanded && bShieldDrawn && IsValid(EquippedShield))
		{
			UE_LOG(LogTemp, Log,
				TEXT("[BWCombatComponent] RefreshCombatState: 양손 무기 장착으로 방패를 즉시 등으로 보관합니다(스냅)."));

			if (OwnerMesh)
			{
				const FName ShieldBackSocket = ShieldBackSocketName;
				if (OwnerMesh->DoesSocketExist(ShieldBackSocket))
				{
					EquippedShield->MoveToHolster(OwnerMesh, ShieldBackSocket);
				}
			}
			bShieldDrawn = false;
		}
	}
	else
	{
		// ── 무기가 없거나 등에 보관 중 → 맨손 ────────────────────────────

		CurrentCombatType = EBWCombatType::MeleeFists;

		// Main 콜리전: 오른손 본 sweep
		if (IsValid(MainCollision) && OwnerMesh)
		{
			MainCollision->ConfigureForFists(OwnerMesh, RightHandBone, FistTraceRadius, FistBaseDamage);
		}
		else if (IsValid(MainCollision))
		{
			MainCollision->ClearSource();
		}

		// Second 콜리전: 왼손 본 sweep
		if (IsValid(SecondCollision) && OwnerMesh)
		{
			SecondCollision->ConfigureForFists(OwnerMesh, LeftHandBone, FistTraceRadius, FistBaseDamage);
		}
		else if (IsValid(SecondCollision))
		{
			SecondCollision->ClearSource();
		}

		// AttackComponent에 맨손 DataTable push
		if (CachedAttackComponent.IsValid())
		{
			CachedAttackComponent->SetActiveAttackDataTable(FistAttackDataTable.Get());
		}
	}
}
