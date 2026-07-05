// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWPotionInventoryComponent.h"

#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/BWAttributeComponent.h"
#include "Combat/BWPotion.h"
#include "Character/BWStateComponent.h"
#include "Core/BWGameplayDefine.h"

UBWPotionInventoryComponent::UBWPotionInventoryComponent()
{
	// 포션 인벤토리는 이벤트 기반으로 처리한다. 매 프레임 틱 비활성(CLAUDE.md 4.1).
	PrimaryComponentTick.bCanEverTick = false;
}

void UBWPotionInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// AttributeComponent를 1회 캐시 — DrinkPotion 호출 시 반복 탐색 방지(CLAUDE.md 4.1).
	AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		CachedAttributeComponent = Owner->FindComponentByClass<UBWAttributeComponent>();
		if (!CachedAttributeComponent.IsValid())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[BWPotionInventoryComponent] BeginPlay: '%s'에서 UBWAttributeComponent를 찾을 수 없습니다. DrinkPotion이 체력 회복을 건너뜁니다."),
				*Owner->GetName());
		}

		// StateComponent를 1회 캐시 — DrinkPotion 진입 시 DrinkingPotion 태그 확인에 사용(CLAUDE.md 4.1).
		// BWAttackComponent/BWCombatComponent의 CachedStateComponent 캐시 패턴과 동일.
		CachedStateComponent = Owner->FindComponentByClass<UBWStateComponent>();
		if (!CachedStateComponent.IsValid())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[BWPotionInventoryComponent] BeginPlay: '%s'에서 UBWStateComponent를 찾을 수 없습니다. DrinkPotion의 재진입 가드가 동작하지 않습니다."),
				*Owner->GetName());
		}
	}

	// 초기 수량을 브로드캐스트한다. 단, HUD 구독은 GameMode::InitializeHUD(다음 틱)에서 이뤄지므로
	// 이 시점의 브로드캐스트에는 수신자가 없다 — InitializeForPlayer에서 명시 호출로 초기 표시를 보장한다.
	BroadcastPotionUpdate();
}

void UBWPotionInventoryComponent::DrinkPotion()
{
	// 재진입/뒤늦은 노티파이 차단 가드.
	// InterruptWhileDrinkingPotion()이 태그를 먼저 해제하므로, 블렌드아웃 구간에서
	// 뒤늦게 발화하는 AnimNotify_BWPotionDrink가 여기서 차단된다.
	// (피격 취소 시 회복 무효화 — 소울라이크 요건)
	if (CachedStateComponent.IsValid()
		&& !CachedStateComponent->HasStateTag(BWGameplayTags::Character_State_DrinkingPotion.GetTag()))
	{
		return;
	}

	if (PotionQuantity <= 0)
	{
		return;
	}

	UBWAttributeComponent* AttributeComp = CachedAttributeComponent.Get();
	if (!IsValid(AttributeComp))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWPotionInventoryComponent] DrinkPotion: AttributeComponent가 유효하지 않아 체력 회복을 건너뜁니다."));
		return;
	}

	// 체력 회복
	AttributeComp->RestoreHealth(PotionHealAmount);

	// 수량 감소 (언더플로 가드)
	PotionQuantity = FMath::Max(0, PotionQuantity - 1);

	// HUD 갱신
	BroadcastPotionUpdate();
}

void UBWPotionInventoryComponent::SpawnPotion()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!PotionClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWPotionInventoryComponent] SpawnPotion: PotionClass가 설정되지 않았습니다. BP_PlayerCharacter의 (상속됨) PotionInventoryComponent에서 PotionClass를 지정하세요."));
		return;
	}

	// 재진입 안전: 이미 존재하는 포션 액터를 먼저 정리한다.
	if (IsValid(PotionActor))
	{
		DespawnPotion();
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(Owner);
	if (!OwnerCharacter)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWPotionInventoryComponent] SpawnPotion: Owner '%s'가 ACharacter가 아닙니다."),
			*Owner->GetName());
		return;
	}

	USkeletalMeshComponent* CharMesh = OwnerCharacter->GetMesh();
	if (!CharMesh)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWPotionInventoryComponent] SpawnPotion: Owner '%s'의 SkeletalMeshComponent를 찾을 수 없습니다."),
			*Owner->GetName());
		return;
	}

	if (PotionSocketName == NAME_None)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWPotionInventoryComponent] SpawnPotion: PotionSocketName이 설정되지 않았습니다. BP_PlayerCharacter의 (상속됨) PotionInventoryComponent에서 소켓 이름을 지정하세요."));
		return;
	}

	// 포션 액터 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	PotionActor = World->SpawnActor<ABWPotion>(PotionClass, FTransform::Identity, SpawnParams);
	if (!IsValid(PotionActor))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWPotionInventoryComponent] SpawnPotion: 포션 액터 스폰에 실패했습니다."));
		return;
	}

	// 캐릭터 손 소켓에 부착
	PotionActor->AttachToComponent(
		CharMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		PotionSocketName);
}

void UBWPotionInventoryComponent::DespawnPotion()
{
	if (!IsValid(PotionActor))
	{
		PotionActor = nullptr;
		return;
	}

	PotionActor->Destroy();
	PotionActor = nullptr;
}

void UBWPotionInventoryComponent::SetPotionQuantity(int32 InQuantity)
{
	PotionQuantity = FMath::Clamp(InQuantity, 0, MaxPotionQuantity);
	BroadcastPotionUpdate();
}

void UBWPotionInventoryComponent::BroadcastPotionUpdate()
{
	OnUpdatePotionAmount.Broadcast(PotionQuantity);
}

bool UBWPotionInventoryComponent::CanDrink() const
{
	return PotionQuantity > 0;
}
