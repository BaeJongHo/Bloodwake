// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWInvincibilityNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Combat/BWCombatInterface.h"

void UBWAnimNotifyState_Invincibility::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventRef);

	IBWCombatInterface* CombatInterface = FindCombatInterface(MeshComp);
	if (!CombatInterface)
	{
		return;
	}

	CombatInterface->SetInvincible(true);
}

void UBWAnimNotifyState_Invincibility::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyEnd(MeshComp, Animation, EventRef);

	IBWCombatInterface* CombatInterface = FindCombatInterface(MeshComp);
	if (!CombatInterface)
	{
		return;
	}

	CombatInterface->SetInvincible(false);
}

FString UBWAnimNotifyState_Invincibility::GetNotifyName_Implementation() const
{
	return TEXT("BW Invincibility Window");
}

// ── private 헬퍼 ─────────────────────────────────────────────────────────────

IBWCombatInterface* UBWAnimNotifyState_Invincibility::FindCombatInterface(
	const USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWInvincibilityNotify] FindCombatInterface: MeshComp가 null입니다."));
		return nullptr;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWInvincibilityNotify] FindCombatInterface: MeshComp 소유 액터가 유효하지 않습니다."));
		return nullptr;
	}

	// UBWCombatInterface(UINTERFACE)를 구현하는 액터는 IBWCombatInterface* 로 Cast 가능.
	IBWCombatInterface* CombatInterface = Cast<IBWCombatInterface>(Owner);
	if (!CombatInterface)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWInvincibilityNotify] FindCombatInterface: 소유 액터(%s)가 IBWCombatInterface를 구현하지 않습니다."),
			*Owner->GetName());
	}

	return CombatInterface;
}
