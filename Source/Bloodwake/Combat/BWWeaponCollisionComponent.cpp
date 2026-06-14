// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWWeaponCollisionComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/DamageEvents.h"
#include "DrawDebugHelpers.h"
#include "Equipment/BWWeapon.h"

UBWWeaponCollisionComponent::UBWWeaponCollisionComponent()
{
	// 평소에는 Tick OFF. StartDetection에서 켜고 StopDetection에서 끈다 (규약 4.1).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
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
	if (!bSourceConfigured)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWWeaponCollisionComponent] StartDetection: sweep 소스가 설정되지 않았습니다. ConfigureForWeapon 또는 ConfigureForFists를 먼저 호출하세요."));
		return;
	}

	// 히트 캐시 초기화
	AlreadyHitActors.Empty();
	bDetecting = true;

	// 첫 프레임이 0→현재 위치로 과도 보간되지 않도록 현재 소켓 위치로 시드한다.
	GetStartSocketWorldLocation(PrevStartLocation);
	GetEndSocketWorldLocation(PrevEndLocation);

	SetComponentTickEnabled(true);
}

void UBWWeaponCollisionComponent::StopDetection()
{
	bDetecting = false;
	SetComponentTickEnabled(false);
	AlreadyHitActors.Empty();
}

void UBWWeaponCollisionComponent::ConfigureForWeapon(UPrimitiveComponent* InWeaponMesh,
	FName InStart, FName InEnd, float InRadius, const ABWWeapon* InWeapon)
{
	SourceMesh = InWeaponMesh;
	SourceStartSocket = InStart;
	SourceEndSocket = InEnd;
	SourceRadius = InRadius;
	SourceWeapon = InWeapon;
	FistDamage = 0.f;
	OwnerCharacter = GetOwner();
	bSourceConfigured = (InWeaponMesh != nullptr);
	bWarnedMissingSocket = false;
}

void UBWWeaponCollisionComponent::ConfigureForFists(USkeletalMeshComponent* InCharacterMesh,
	FName InHandBone, float InRadius, float InFistDamage)
{
	SourceMesh = InCharacterMesh;
	SourceStartSocket = InHandBone;
	SourceEndSocket = InHandBone; // 맨손은 단일 점 sweep — Start==End(의도된 설계: 구체 1개로 처리)
	SourceRadius = InRadius;
	SourceWeapon = nullptr;
	FistDamage = InFistDamage;
	OwnerCharacter = GetOwner();
	bSourceConfigured = (InCharacterMesh != nullptr);
	bWarnedMissingSocket = false;
}

void UBWWeaponCollisionComponent::ClearSource()
{
	SourceMesh = nullptr;
	SourceStartSocket = NAME_None;
	SourceEndSocket = NAME_None;
	SourceRadius = 8.f;
	SourceWeapon = nullptr;
	FistDamage = 0.f;
	bSourceConfigured = false;
	bWarnedMissingSocket = false;

	// 진행 중이었다면 중단
	if (bDetecting)
	{
		StopDetection();
	}
}

// ── private 구현부 ─────────────────────────────────────────────────────────────

void UBWWeaponCollisionComponent::PerformSweep()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!bSourceConfigured || !SourceMesh.IsValid())
	{
		return;
	}

	// 현재 프레임 소켓/본 위치 획득
	FVector CurrStartLocation;
	FVector CurrEndLocation;
	if (!GetStartSocketWorldLocation(CurrStartLocation) ||
		!GetEndSocketWorldLocation(CurrEndLocation))
	{
		return;
	}

	// Sweep 무시 액터 목록: 캐릭터 자신 + (무기 모드면) 무기 액터
	TArray<AActor*> ActorsToIgnore;
	if (OwnerCharacter.IsValid())
	{
		ActorsToIgnore.Add(OwnerCharacter.Get());
	}
	if (SourceWeapon.IsValid())
	{
		// SourceWeapon은 const ABWWeapon*(데미지 조회 전용). AddIgnoredActors는 AActor*(비const)를 요구하므로
		// const_cast로 한정자를 제거한다. 무시 목록에만 사용되며 무기 상태를 변경하지 않는다.
		ActorsToIgnore.Add(const_cast<ABWWeapon*>(SourceWeapon.Get()));
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponCollisionSweep), /*bTraceComplex=*/false);
	QueryParams.AddIgnoredActors(ActorsToIgnore);

	// 이전 프레임 위치 → 현재 프레임 위치 보간 Sweep (관통 방지)
	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(
		HitResults,
		PrevStartLocation,
		CurrStartLocation,
		FQuat::Identity,
		TraceChannel,
		FCollisionShape::MakeSphere(SourceRadius),
		QueryParams
	);

	// 맨손 모드는 Start==End이므로 두 번째 sweep은 불필요하다.
	// 무기 모드는 끝 소켓(칼끝)도 sweep해 선단 관통을 방지한다.
	if (SourceStartSocket != SourceEndSocket)
	{
		TArray<FHitResult> HitResultsEnd;
		World->SweepMultiByChannel(
			HitResultsEnd,
			PrevEndLocation,
			CurrEndLocation,
			FQuat::Identity,
			TraceChannel,
			FCollisionShape::MakeSphere(SourceRadius),
			QueryParams
		);
		HitResults.Append(HitResultsEnd);
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugLine(World, PrevStartLocation, CurrStartLocation, FColor::Red, false, 1.f, 0, 2.f);
		if (SourceStartSocket != SourceEndSocket)
		{
			DrawDebugLine(World, PrevEndLocation, CurrEndLocation, FColor::Orange, false, 1.f, 0, 2.f);
			DrawDebugSphere(World, CurrEndLocation, SourceRadius, 8, FColor::Orange, false, 1.f);
		}
		DrawDebugSphere(World, CurrStartLocation, SourceRadius, 8, FColor::Red, false, 1.f);
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

		// 이미 이번 윈도우에서 타격한 액터는 중복 처리하지 않는다.
		if (AlreadyHitActors.Contains(HitActor))
		{
			continue;
		}

		AlreadyHitActors.Add(HitActor);
		ApplyDamageTo(Hit);
	}

	// 이전 위치 업데이트 — 히트 처리 루프 뒤에서 갱신해야
	// ApplyDamageTo 내부의 ShotDirection 계산이 "이전→현재" 이동 방향을 정확히 참조한다.
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

	// 데미지 소스: 무기 모드면 무기 BaseDamage, 맨손이면 FistDamage.
	float Damage = 0.f;
	if (SourceWeapon.IsValid())
	{
		Damage = SourceWeapon->GetBaseDamage();
	}
	else
	{
		Damage = FistDamage;
	}

	if (Damage <= 0.f)
	{
		return;
	}

	// 공격 진행 방향 = (Sweep 시작→끝) 방향으로 근사한다.
	FVector ShotDirection = PrevEndLocation - PrevStartLocation;
	if (!ShotDirection.IsNearlyZero())
	{
		ShotDirection.Normalize();
	}
	else
	{
		// 소켓이 겹쳐 있으면(맨손 등) 캐릭터 → 피격 액터 방향으로 폴백
		if (OwnerCharacter.IsValid())
		{
			ShotDirection = DamagedActor->GetActorLocation() - OwnerCharacter->GetActorLocation();
			ShotDirection.Normalize();
		}
	}

	// 무기를 휘두르는 캐릭터(EventInstigator 용)
	AController* Instigator = nullptr;
	if (OwnerCharacter.IsValid())
	{
		if (APawn* Pawn = Cast<APawn>(OwnerCharacter.Get()))
		{
			Instigator = Pawn->GetController();
		}
	}

	// DamageCauser: 캐릭터 자신을 항상 사용한다.
	// SourceWeapon은 const 참조이므로 AActor*(비const)가 필요한 TakeDamage에 넘길 수 없다.
	// TakeDamage/FPointDamageEvent의 DamageCauser는 Hit.BoneName·위치 등으로 이미 맥락이 전달되므로
	// OwnerCharacter를 넘겨도 데미지 처리에 충분하다.
	AActor* DamageCauser = OwnerCharacter.IsValid() ? OwnerCharacter.Get() : nullptr;

	FPointDamageEvent DamageEvent(Damage, Hit, ShotDirection, DamageTypeClass);
	DamagedActor->TakeDamage(Damage, DamageEvent, Instigator, DamageCauser);
}

bool UBWWeaponCollisionComponent::GetStartSocketWorldLocation(FVector& OutLocation) const
{
	if (!SourceMesh.IsValid())
	{
		OutLocation = FVector::ZeroVector;
		return false;
	}

	if (SourceMesh->DoesSocketExist(SourceStartSocket))
	{
		OutLocation = SourceMesh->GetSocketLocation(SourceStartSocket);
		return true;
	}

	// 소켓/본이 없으면 컴포넌트 위치로 폴백. 매 틱 로그 폭발을 막기 위해 첫 발생 시 1회만 경고한다.
	if (!bWarnedMissingSocket)
	{
		bWarnedMissingSocket = true;
		UE_LOG(LogTemp, Warning,
			TEXT("[BWWeaponCollisionComponent] GetStartSocketWorldLocation: 소켓/본 '%s'가 메시(%s)에 없습니다. 컴포넌트 위치로 폴백합니다."),
			*SourceStartSocket.ToString(), *SourceMesh->GetName());
	}

	OutLocation = SourceMesh->GetComponentLocation();
	return false;
}

bool UBWWeaponCollisionComponent::GetEndSocketWorldLocation(FVector& OutLocation) const
{
	if (!SourceMesh.IsValid())
	{
		OutLocation = FVector::ZeroVector;
		return false;
	}

	if (SourceMesh->DoesSocketExist(SourceEndSocket))
	{
		OutLocation = SourceMesh->GetSocketLocation(SourceEndSocket);
		return true;
	}

	// 소켓/본이 없으면 컴포넌트 위치로 폴백. 매 틱 로그 폭발을 막기 위해 첫 발생 시 1회만 경고한다.
	if (!bWarnedMissingSocket)
	{
		bWarnedMissingSocket = true;
		UE_LOG(LogTemp, Warning,
			TEXT("[BWWeaponCollisionComponent] GetEndSocketWorldLocation: 소켓/본 '%s'가 메시(%s)에 없습니다. 컴포넌트 위치로 폴백합니다."),
			*SourceEndSocket.ToString(), *SourceMesh->GetName());
	}

	OutLocation = SourceMesh->GetComponentLocation();
	return false;
}
