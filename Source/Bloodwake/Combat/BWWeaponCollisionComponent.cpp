// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWWeaponCollisionComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/DamageEvents.h"
#include "DrawDebugHelpers.h"
#include "Equipment/BWEquipItem.h"
#include "Equipment/BWWeapon.h"
#include "Combat/BWCombatComponent.h"

UBWWeaponCollisionComponent::UBWWeaponCollisionComponent()
{
	// 평소에는 Tick OFF. StartDetection 에서 켜고 StopDetection 에서 끈다 (규약 4.1).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UBWWeaponCollisionComponent::BeginPlay()
{
	Super::BeginPlay();

	// 무기 루트 StaticMeshComponent를 1회 캐시한다. 매 틱 FindComponentByClass 호출 방지.
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWWeaponCollisionComponent] BeginPlay: Owner가 유효하지 않습니다."));
		return;
	}

	UStaticMeshComponent* WeaponMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
	if (!WeaponMesh)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWWeaponCollisionComponent] BeginPlay: Owner(%s)에서 UStaticMeshComponent를 찾지 못했습니다. 소켓 Sweep이 작동하지 않을 수 있습니다."),
			*Owner->GetName());
	}
	CachedWeaponMesh = WeaponMesh;
}

void UBWWeaponCollisionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDetecting)
	{
		PerformSweep();
	}
}

void UBWWeaponCollisionComponent::StartDetection()
{
	// 히트 캐시 초기화
	AlreadyHitActors.Empty();
	bDetecting = true;

	// 첫 프레임이 0→현재 위치로 과도 보간되지 않도록 현재 소켓 위치로 시드한다.
	GetSocketWorldLocation(TraceStartSocket, PrevStartLocation);
	GetSocketWorldLocation(TraceEndSocket, PrevEndLocation);

	SetComponentTickEnabled(true);
}

void UBWWeaponCollisionComponent::StopDetection()
{
	bDetecting = false;
	SetComponentTickEnabled(false);
	AlreadyHitActors.Empty();
}

UBWWeaponCollisionComponent* UBWWeaponCollisionComponent::FindOnEquippedWeapon(
	const USkeletalMeshComponent* MeshComp)
{
	if (!MeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWWeaponCollisionComponent] FindOnEquippedWeapon: MeshComp가 null입니다."));
		return nullptr;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWWeaponCollisionComponent] FindOnEquippedWeapon: MeshComp 소유 액터가 유효하지 않습니다."));
		return nullptr;
	}

	// 캐릭터에서 CombatComponent → 장착 무기 → WeaponCollisionComponent 탐색.
	UBWCombatComponent* CombatComp = Owner->FindComponentByClass<UBWCombatComponent>();
	if (!CombatComp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWWeaponCollisionComponent] FindOnEquippedWeapon: 소유 액터(%s)에서 UBWCombatComponent를 찾지 못했습니다."),
			*Owner->GetName());
		return nullptr;
	}

	ABWEquipItem* EquippedWeapon = CombatComp->GetEquippedWeapon();
	if (!IsValid(EquippedWeapon))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWWeaponCollisionComponent] FindOnEquippedWeapon: 소유 액터(%s)에 장착된 무기가 없습니다."),
			*Owner->GetName());
		return nullptr;
	}

	UBWWeaponCollisionComponent* CollisionComp =
		EquippedWeapon->FindComponentByClass<UBWWeaponCollisionComponent>();
	if (!CollisionComp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWWeaponCollisionComponent] FindOnEquippedWeapon: 무기(%s)에서 UBWWeaponCollisionComponent를 찾지 못했습니다."),
			*EquippedWeapon->GetName());
	}

	return CollisionComp;
}

// ── private 구현부 ─────────────────────────────────────────────────────────────

void UBWWeaponCollisionComponent::PerformSweep()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 현재 프레임 소켓 위치 획득
	FVector CurrStartLocation;
	FVector CurrEndLocation;
	if (!GetSocketWorldLocation(TraceStartSocket, CurrStartLocation) ||
		!GetSocketWorldLocation(TraceEndSocket, CurrEndLocation))
	{
		// 위치 획득 실패 시 이전 위치 유지 후 다음 프레임에서 재시도
		return;
	}

	// Sweep 무시 액터 목록: 무기 자신 + 무기를 소유한 캐릭터
	TArray<AActor*> ActorsToIgnore;
	if (AActor* WeaponOwner = GetOwner())
	{
		ActorsToIgnore.Add(WeaponOwner);
		if (AActor* CharacterOwner = WeaponOwner->GetOwner())
		{
			ActorsToIgnore.Add(CharacterOwner);
		}
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponCollisionSweep), /*bTraceComplex=*/false);
	QueryParams.AddIgnoredActors(ActorsToIgnore);

	// 이전 프레임 Start→End 에서 현재 프레임 Start→End 로 보간 Sweep (관통 방지)
	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(
		HitResults,
		PrevStartLocation,
		CurrStartLocation,
		FQuat::Identity,
		TraceChannel,
		FCollisionShape::MakeSphere(TraceRadius),
		QueryParams
	);

	// End 소켓 쪽도 Sweep (칼끝 선단 보정)
	TArray<FHitResult> HitResultsEnd;
	World->SweepMultiByChannel(
		HitResultsEnd,
		PrevEndLocation,
		CurrEndLocation,
		FQuat::Identity,
		TraceChannel,
		FCollisionShape::MakeSphere(TraceRadius),
		QueryParams
	);
	HitResults.Append(HitResultsEnd);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugLine(World, PrevStartLocation, CurrStartLocation, FColor::Red, false, 1.f, 0, 2.f);
		DrawDebugLine(World, PrevEndLocation, CurrEndLocation, FColor::Orange, false, 1.f, 0, 2.f);
		DrawDebugSphere(World, CurrStartLocation, TraceRadius, 8, FColor::Red, false, 1.f);
		DrawDebugSphere(World, CurrEndLocation, TraceRadius, 8, FColor::Orange, false, 1.f);
	}
#endif

	// 적중 결과 처리
	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!IsValid(HitActor))
		{
			continue;
		}

		// 이미 이번 윈도우에서 타격한 액터는 중복 처리하지 않는다
		if (AlreadyHitActors.Contains(HitActor))
		{
			continue;
		}

		AlreadyHitActors.Add(HitActor);
		ApplyDamageTo(Hit);
	}

	// 이전 위치 업데이트 — 히트 처리 루프 뒤에서 갱신해야
	// ApplyDamageTo 내부의 PrevEndLocation - PrevStartLocation 계산이
	// "이전 프레임 → 현재 프레임" 이동 방향을 정확히 참조한다.
	PrevStartLocation = CurrStartLocation;
	PrevEndLocation = CurrEndLocation;
}

void UBWWeaponCollisionComponent::ApplyDamageTo(const FHitResult& Hit)
{
	AActor* DamagedActor = Hit.GetActor();
	if (!IsValid(DamagedActor))
	{
		return;
	}

	// 데미지 수치는 무기(ABWWeapon)에서 읽는다 — 단일 진실 공급원.
	ABWWeapon* Weapon = Cast<ABWWeapon>(GetOwner());
	if (!Weapon)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWWeaponCollisionComponent] ApplyDamageTo: Owner가 ABWWeapon이 아닙니다. 데미지 0으로 처리합니다."));
		return;
	}

	const float Damage = Weapon->GetBaseDamage();
	if (Damage <= 0.f)
	{
		return;
	}

	// 공격 진행 방향 = (현재 Sweep 시작→끝) 방향으로 근사한다.
	FVector ShotDirection = (PrevEndLocation - PrevStartLocation);
	if (!ShotDirection.IsNearlyZero())
	{
		ShotDirection.Normalize();
	}
	else
	{
		// 소켓이 겹쳐 있으면 무기 소유자 → 피격 액터 방향으로 폴백
		if (AActor* WeaponOwner = GetOwner())
		{
			ShotDirection = (DamagedActor->GetActorLocation() - WeaponOwner->GetActorLocation());
			ShotDirection.Normalize();
		}
	}

	// 무기를 휘두르는 캐릭터(EventInstigator 용)
	AController* Instigator = nullptr;
	if (AActor* WeaponOwner = GetOwner())
	{
		if (AActor* CharacterOwner = WeaponOwner->GetOwner())
		{
			if (APawn* Pawn = Cast<APawn>(CharacterOwner))
			{
				Instigator = Pawn->GetController();
			}
		}
	}

	FPointDamageEvent DamageEvent(Damage, Hit, ShotDirection, DamageTypeClass);
	DamagedActor->TakeDamage(Damage, DamageEvent, Instigator, GetOwner());
}

bool UBWWeaponCollisionComponent::GetSocketWorldLocation(FName SocketName, FVector& OutLocation) const
{
	UStaticMeshComponent* WeaponMesh = CachedWeaponMesh.Get();
	if (!WeaponMesh)
	{
		// 캐시가 없으면 액터 위치로 폴백
		if (const AActor* Owner = GetOwner())
		{
			OutLocation = Owner->GetActorLocation();
		}
		return false;
	}

	// 소켓이 있으면 소켓 월드 위치, 없으면 액터 위치로 폴백 + 1회 Warning
	if (WeaponMesh->DoesSocketExist(SocketName))
	{
		OutLocation = WeaponMesh->GetSocketLocation(SocketName);
		return true;
	}

	// 소켓 없음 경고는 첫 번째 호출 시에만 1회 출력 (매 틱 반복 방지를 위해 ensureMsgf 활용)
	ensureMsgf(false,
		TEXT("[BWWeaponCollisionComponent] GetSocketWorldLocation: 소켓 '%s'가 무기 메시(%s)에 없습니다. 액터 위치로 폴백합니다."),
		*SocketName.ToString(), *WeaponMesh->GetName());

	OutLocation = WeaponMesh->GetComponentLocation();
	return false;
}
