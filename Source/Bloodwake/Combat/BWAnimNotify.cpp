// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWAnimNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Combat/BWCombatComponent.h"

// ── UBWAnimNotify ─────────────────────────────────────────────────────────────

UBWCombatComponent* UBWAnimNotify::GetCombatComponent(const USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWAnimNotify] GetCombatComponent: MeshComp가 null입니다."));
		return nullptr;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWAnimNotify] GetCombatComponent: MeshComp 소유 액터가 유효하지 않습니다."));
		return nullptr;
	}

	UBWCombatComponent* CombatComp = Owner->FindComponentByClass<UBWCombatComponent>();
	if (!CombatComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWAnimNotify] GetCombatComponent: 소유 액터(%s)에서 UBWCombatComponent를 찾지 못했습니다."), *Owner->GetName());
	}

	return CombatComp;
}

// ── UBWAnimNotify_AttachEquip ─────────────────────────────────────────────────

void UBWAnimNotify_AttachEquip::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                       const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UBWCombatComponent* CombatComp = GetCombatComponent(MeshComp);
	if (!CombatComp)
	{
		return;
	}

	CombatComp->AttachSlotToSocket(TargetSlot, Destination);
}

FString UBWAnimNotify_AttachEquip::GetNotifyName_Implementation() const
{
	const FString SlotName = (TargetSlot == EBWEquipSlot::Weapon) ? TEXT("Weapon") : TEXT("Shield");
	const FString DestName = (Destination == EBWAttachDestination::Hand) ? TEXT("Hand") : TEXT("Back");
	return FString::Printf(TEXT("AttachEquip: %s→%s"), *SlotName, *DestName);
}
