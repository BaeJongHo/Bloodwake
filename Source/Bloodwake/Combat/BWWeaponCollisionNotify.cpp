// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWWeaponCollisionNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "Combat/BWWeaponCollisionComponent.h"

void UBWAnimNotifyState_WeaponCollision::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventRef);

	UBWWeaponCollisionComponent* CollisionComp =
		UBWWeaponCollisionComponent::FindOnEquippedWeapon(MeshComp);
	if (!CollisionComp)
	{
		return;
	}

	CollisionComp->StartDetection();
}

void UBWAnimNotifyState_WeaponCollision::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyEnd(MeshComp, Animation, EventRef);

	UBWWeaponCollisionComponent* CollisionComp =
		UBWWeaponCollisionComponent::FindOnEquippedWeapon(MeshComp);
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
