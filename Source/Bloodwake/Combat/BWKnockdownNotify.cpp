// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWKnockdownNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "Engine/DamageEvents.h"
#include "DrawDebugHelpers.h"
#include "Combat/BWDamageTypes.h"

UBWAnimNotify_BWKnockdown::UBWAnimNotify_BWKnockdown()
{
	// 기본 데미지 타입을 넉다운으로 설정한다(애니메이터가 덮어쓸 수 있음).
	DamageTypeClass = UBWDamageType_Knockdown::StaticClass();
}

void UBWAnimNotify_BWKnockdown::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// 1. null 가드 — 에디터 애님 프리뷰에서도 크래시 없이 조용히 return
	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	UWorld* World = MeshComp->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	// 2. 소켓 위치 결정 (소켓 없으면 액터 위치 폴백 + Warning)
	FVector SocketLocation;
	if (SocketName != NAME_None && MeshComp->DoesSocketExist(SocketName))
	{
		SocketLocation = MeshComp->GetSocketLocation(SocketName);
	}
	else
	{
		SocketLocation = Owner->GetActorLocation();
		if (SocketName != NAME_None)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[BWKnockdownNotify] 소켓 '%s'을(를) '%s'에서 찾을 수 없습니다. 액터 위치로 폴백합니다."),
				*SocketName.ToString(), *Owner->GetName());
		}
	}

	// 3. 구 오버랩 — 생성자 인자에 Owner를 넘겨 소유 액터를 질의 단계에서 제외한다
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BWKnockdown), /*bTraceComplex=*/false, Owner);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(
		Overlaps,
		SocketLocation,
		FQuat::Identity,
		TraceChannel,
		FCollisionShape::MakeSphere(Radius),
		QueryParams
	);

	// 4. 결과 순회 — 액터 단위 중복 제거 (한 액터의 여러 컴포넌트가 겹칠 수 있음)
	TSet<AActor*> DamagedActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!IsValid(HitActor))
		{
			continue;
		}

		// 소유자 이중 제외 (QueryParams에서 제외했지만 방어적으로 재확인)
		if (HitActor == Owner)
		{
			continue;
		}

		if (DamagedActors.Contains(HitActor))
		{
			continue;
		}

		DamagedActors.Add(HitActor);

		// 5. 데미지 이벤트 구성 및 적용
		// ShotDirection: 노티파이 발사 위치 → 피격 액터 방향. GetSafeNormal로 0 벡터 보호.
		const FVector ShotDirection = (HitActor->GetActorLocation() - SocketLocation).GetSafeNormal();

		// ImpactPoint: ABWPlayerCharacter::TakeDamage가 VFX/사운드 위치로 PointDamageEvent.HitInfo.ImpactPoint를 읽는다.
		FHitResult Hit;
		Hit.ImpactPoint = HitActor->GetActorLocation();
		Hit.Location    = HitActor->GetActorLocation();

		// EventInstigator: Owner가 Pawn이면 그 컨트롤러를 전달한다.
		AController* EventInstigator = nullptr;
		if (APawn* PawnOwner = Cast<APawn>(Owner))
		{
			EventInstigator = PawnOwner->GetController();
		}

		// DamageCauser = Owner(보스 액터). ABWPlayerCharacter::ComputeHitDirection이 공격자 위치 기준으로
		// 4방향을 분류하므로 소유 액터를 정확히 전달한다(BWWeaponCollisionComponent::ApplyDamageTo와 동일 정책).
		const TSubclassOf<UDamageType> EffectiveType = DamageTypeClass
			? DamageTypeClass
			: TSubclassOf<UDamageType>(UDamageType::StaticClass());

		FPointDamageEvent DamageEvent(Damage, Hit, ShotDirection, EffectiveType);
		HitActor->TakeDamage(Damage, DamageEvent, EventInstigator, Owner);
	}

	// 6. 디버그 드로우 (ENABLE_DRAW_DEBUG 블록으로 쉬핑 빌드에서 제거됨)
#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(World, SocketLocation, Radius, 16, FColor::Red, /*bPersistentLines=*/false, /*LifeTime=*/1.0f);
	}
#endif
}

FString UBWAnimNotify_BWKnockdown::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Knockdown: %s R=%.0f"),
		(SocketName != NAME_None) ? *SocketName.ToString() : TEXT("ActorLoc"),
		Radius);
}
