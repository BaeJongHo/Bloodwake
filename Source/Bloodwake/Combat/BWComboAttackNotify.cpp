// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWComboAttackNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "Combat/BWAttackComponent.h"

// ── UBWComboAttackWindowNotifyState ──────────────────────────────────────────

void UBWComboAttackWindowNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventRef);

	UBWAttackComponent* AttackComp = UBWAttackComponent::FindAttackComponent(MeshComp);
	if (!AttackComp)
	{
		return;
	}

	AttackComp->OpenComboWindow();
}

void UBWComboAttackWindowNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyEnd(MeshComp, Animation, EventRef);

	UBWAttackComponent* AttackComp = UBWAttackComponent::FindAttackComponent(MeshComp);
	if (!AttackComp)
	{
		return;
	}

	AttackComp->CloseComboWindow();
}

FString UBWComboAttackWindowNotifyState::GetNotifyName_Implementation() const
{
	return TEXT("ComboWindow");
}

// ── UBWComboAttackEndNotify ───────────────────────────────────────────────────

void UBWComboAttackEndNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventRef)
{
	Super::Notify(MeshComp, Animation, EventRef);

	UBWAttackComponent* AttackComp = UBWAttackComponent::FindAttackComponent(MeshComp);
	if (!AttackComp)
	{
		return;
	}

	AttackComp->NotifyComboEnd();
}

FString UBWComboAttackEndNotify::GetNotifyName_Implementation() const
{
	return TEXT("ComboEnd");
}
