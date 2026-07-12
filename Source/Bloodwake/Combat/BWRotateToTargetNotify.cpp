// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWRotateToTargetNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "AI/BWEnemyAIController.h"

void UBWAnimNotifyState_RotateToTarget::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	// 소유 액터 null 가드
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!IsValid(Owner))
	{
		return;
	}

	// 현재 타깃 조회 — 에디터 프리뷰·컨트롤러 없음 시 nullptr 반환
	AActor* Target = ResolveTarget(MeshComp);
	if (!IsValid(Target))
	{
		return;
	}

	// 수평 방향 벡터 계산 (Z 성분 제거 — 순수 yaw 회전)
	FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	// 목표 회전 — Pitch·Roll은 0으로 클램프해 순수 yaw 정렬만 수행한다.
	FRotator Desired = ToTarget.Rotation();
	Desired.Pitch = 0.f;
	Desired.Roll  = 0.f;

	// RInterpTo로 부드럽게 보간
	const FRotator NewRot = FMath::RInterpTo(
		Owner->GetActorRotation(), Desired, FrameDeltaTime, RotationInterpSpeed);

	Owner->SetActorRotation(NewRot);
}

FString UBWAnimNotifyState_RotateToTarget::GetNotifyName_Implementation() const
{
	return TEXT("RotateToTarget");
}

AActor* UBWAnimNotifyState_RotateToTarget::ResolveTarget(const USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		return nullptr;
	}

	// 소유 폰을 통해 ABWEnemyAIController를 조회한다.
	// Cast 실패(에디터 프리뷰, 플레이어 캐릭터, 다른 AI 컨트롤러) 시 nullptr — 안전.
	APawn* Pawn = Cast<APawn>(MeshComp->GetOwner());
	if (!IsValid(Pawn))
	{
		return nullptr;
	}

	ABWEnemyAIController* AIController = Cast<ABWEnemyAIController>(Pawn->GetController());
	if (!IsValid(AIController))
	{
		return nullptr;
	}

	return AIController->GetCurrentTarget();
}
