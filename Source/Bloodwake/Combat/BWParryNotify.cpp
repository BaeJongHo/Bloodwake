// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWParryNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "Character/BWStateComponent.h"
#include "Core/BWGameplayDefine.h"

// ── UBWAnimNotifyState_BWParry ────────────────────────────────────────────────

void UBWAnimNotifyState_BWParry::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventRef);

	UBWStateComponent* StateComp = FindStateComponent(MeshComp);
	if (!StateComp)
	{
		return;
	}

	StateComp->AddStateTag(BWGameplayTags::Character_State_Parrying.GetTag());
}

void UBWAnimNotifyState_BWParry::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyEnd(MeshComp, Animation, EventRef);

	UBWStateComponent* StateComp = FindStateComponent(MeshComp);
	if (!StateComp)
	{
		return;
	}

	StateComp->RemoveStateTag(BWGameplayTags::Character_State_Parrying.GetTag());
}

FString UBWAnimNotifyState_BWParry::GetNotifyName_Implementation() const
{
	return TEXT("ParryWindow");
}

UBWStateComponent* UBWAnimNotifyState_BWParry::FindStateComponent(const USkeletalMeshComponent* MeshComp)
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

	UBWStateComponent* StateComp = Owner->FindComponentByClass<UBWStateComponent>();
	if (!StateComp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWParryNotify] FindStateComponent: '%s'에서 UBWStateComponent를 찾을 수 없습니다."),
			*Owner->GetName());
	}

	return StateComp;
}
