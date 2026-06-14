// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWWeaponCollisionNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "Combat/BWCombatComponent.h"
#include "Combat/BWWeaponCollisionComponent.h"

void UBWAnimNotifyState_WeaponCollision::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventRef);

	UBWCombatComponent* CombatComp = FindCombatComponent(MeshComp);
	if (!CombatComp)
	{
		return;
	}

	UBWWeaponCollisionComponent* CollisionComp = CombatComp->GetCollisionForSlot(TargetSlot);
	if (!CollisionComp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWWeaponCollisionNotify] NotifyBegin: GetCollisionForSlot이 nullptr를 반환했습니다. TargetSlot=%d"),
			static_cast<int32>(TargetSlot));
		return;
	}

	CollisionComp->StartDetection();
}

void UBWAnimNotifyState_WeaponCollision::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyEnd(MeshComp, Animation, EventRef);

	UBWCombatComponent* CombatComp = FindCombatComponent(MeshComp);
	if (!CombatComp)
	{
		return;
	}

	UBWWeaponCollisionComponent* CollisionComp = CombatComp->GetCollisionForSlot(TargetSlot);
	if (!CollisionComp)
	{
		return;
	}

	CollisionComp->StopDetection();
}

FString UBWAnimNotifyState_WeaponCollision::GetNotifyName_Implementation() const
{
	return TEXT("WeaponCollision");
}

// ── private 헬퍼 ─────────────────────────────────────────────────────────────

UBWCombatComponent* UBWAnimNotifyState_WeaponCollision::FindCombatComponent(
	const USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWWeaponCollisionNotify] FindCombatComponent: MeshComp가 null입니다."));
		return nullptr;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWWeaponCollisionNotify] FindCombatComponent: MeshComp 소유 액터가 유효하지 않습니다."));
		return nullptr;
	}

	UBWCombatComponent* CombatComp = Owner->FindComponentByClass<UBWCombatComponent>();
	if (!CombatComp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWWeaponCollisionNotify] FindCombatComponent: 소유 액터(%s)에서 UBWCombatComponent를 찾지 못했습니다."),
			*Owner->GetName());
	}

	return CombatComp;
}
