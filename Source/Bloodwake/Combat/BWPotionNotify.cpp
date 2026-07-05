// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWPotionNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "Combat/BWPotionInventoryComponent.h"

// ── UBWAnimNotifyState_BWPotionSpawn ─────────────────────────────────────────

void UBWAnimNotifyState_BWPotionSpawn::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventRef);

	UBWPotionInventoryComponent* PotionInventory = FindPotionInventory(MeshComp);
	if (!PotionInventory)
	{
		return;
	}

	PotionInventory->SpawnPotion();
}

void UBWAnimNotifyState_BWPotionSpawn::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyEnd(MeshComp, Animation, EventRef);

	UBWPotionInventoryComponent* PotionInventory = FindPotionInventory(MeshComp);
	if (!PotionInventory)
	{
		return;
	}

	PotionInventory->DespawnPotion();
}

FString UBWAnimNotifyState_BWPotionSpawn::GetNotifyName_Implementation() const
{
	return TEXT("PotionSpawn");
}

UBWPotionInventoryComponent* UBWAnimNotifyState_BWPotionSpawn::FindPotionInventory(const USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		return nullptr;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		return nullptr;
	}

	UBWPotionInventoryComponent* PotionInventory = Owner->FindComponentByClass<UBWPotionInventoryComponent>();
	if (!PotionInventory)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWPotionNotify] FindPotionInventory: '%s'에서 UBWPotionInventoryComponent를 찾을 수 없습니다."),
			*Owner->GetName());
	}

	return PotionInventory;
}

// ── UBWAnimNotify_BWPotionDrink ───────────────────────────────────────────────

void UBWAnimNotify_BWPotionDrink::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UBWPotionInventoryComponent* PotionInventory = FindPotionInventory(MeshComp);
	if (!PotionInventory)
	{
		return;
	}

	PotionInventory->DrinkPotion();
}

FString UBWAnimNotify_BWPotionDrink::GetNotifyName_Implementation() const
{
	return TEXT("PotionDrink");
}

UBWPotionInventoryComponent* UBWAnimNotify_BWPotionDrink::FindPotionInventory(const USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		return nullptr;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		return nullptr;
	}

	UBWPotionInventoryComponent* PotionInventory = Owner->FindComponentByClass<UBWPotionInventoryComponent>();
	if (!PotionInventory)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWPotionNotify] FindPotionInventory: '%s'에서 UBWPotionInventoryComponent를 찾을 수 없습니다."),
			*Owner->GetName());
	}

	return PotionInventory;
}
