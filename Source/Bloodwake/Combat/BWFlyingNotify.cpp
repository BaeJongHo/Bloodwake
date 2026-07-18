// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWFlyingNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/BWStateComponent.h"
#include "Core/BWGameplayDefine.h"

// ── NotifyBegin ──────────────────────────────────────────────────────────────

void UBWAnimNotifyState_BWFlying::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventRef);

	ACharacter* Owner = FindOwnerCharacter(MeshComp);
	if (!Owner)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	// MOVE_Flying으로 전환: 루트모션 상승/체공 중 중력이 작용하지 않는다.
	MoveComp->SetMovementMode(MOVE_Flying);

	// Character.Attack.Air 태그 부착 — 체공 상태를 외부에서 질의 가능하게 표시한다.
	UBWStateComponent* StateComp = FindStateComponent(MeshComp);
	if (StateComp)
	{
		StateComp->AddStateTag(BWGameplayTags::Character_Attack_Air.GetTag());
	}
}

// ── NotifyEnd ────────────────────────────────────────────────────────────────

void UBWAnimNotifyState_BWFlying::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventRef)
{
	Super::NotifyEnd(MeshComp, Animation, EventRef);

	ACharacter* Owner = FindOwnerCharacter(MeshComp);
	if (!Owner)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Owner->GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	// MOVE_Walking 복귀: 공중에 떠 있으면 PhysWalking이 바닥 미검출로 자동 MOVE_Falling 전환해
	// 자연스럽게 착지한다.
	MoveComp->SetMovementMode(MOVE_Walking);

	// Character.Attack.Air 태그 해제 — NotifyBegin과 정확히 대칭.
	UBWStateComponent* StateComp = FindStateComponent(MeshComp);
	if (StateComp)
	{
		StateComp->RemoveStateTag(BWGameplayTags::Character_Attack_Air.GetTag());
	}
}

FString UBWAnimNotifyState_BWFlying::GetNotifyName_Implementation() const
{
	return TEXT("Flying (Air Attack)");
}

// ── private 헬퍼 ─────────────────────────────────────────────────────────────

ACharacter* UBWAnimNotifyState_BWFlying::FindOwnerCharacter(const USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		return nullptr;
	}

	// IsValid로 에디터 프리뷰 환경(컨트롤러/월드 없음) 보호
	AActor* Actor = MeshComp->GetOwner();
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	return Cast<ACharacter>(Actor);
}

UBWStateComponent* UBWAnimNotifyState_BWFlying::FindStateComponent(const USkeletalMeshComponent* MeshComp)
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
		UE_LOG(LogTemp, Verbose,
			TEXT("[BWFlyingNotify] FindStateComponent: '%s'에서 UBWStateComponent를 찾을 수 없습니다."),
			*Owner->GetName());
	}

	return StateComp;
}
