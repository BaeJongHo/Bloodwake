// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BWWeaponCollisionComponent.generated.h"

class UStaticMeshComponent;
class UDamageType;

/**
 * 무기 액터에 부착하는 히트 판정 컴포넌트.
 * 감지 ON 동안(StartDetection~StopDetection) 매 틱 Start/End 소켓 사이를 CapsuleSweep 하여
 * 피격 액터를 검출하고 TakeDamage(FPointDamageEvent)로 데미지를 전달한다.
 * 한 감지 윈도우 내 동일 액터 중복 타격을 방지(AlreadyHitActors 캐시).
 * 히트 윈도우 제어는 UBWAnimNotifyState_WeaponCollision 이 담당한다.
 * 1단계: GAS 미적용, 싱글플레이 전용.
 */
UCLASS(ClassGroup = (Bloodwake), meta = (BlueprintSpawnableComponent))
class BLOODWAKE_API UBWWeaponCollisionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBWWeaponCollisionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * 히트 윈도우 시작. UBWAnimNotifyState_WeaponCollision::NotifyBegin 에서 호출한다.
	 * AlreadyHitActors 초기화, PrevLocation 소켓 위치로 시드, Tick ON.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|WeaponCollision")
	void StartDetection();

	/**
	 * 히트 윈도우 종료. UBWAnimNotifyState_WeaponCollision::NotifyEnd 에서 호출한다.
	 * Tick OFF, AlreadyHitActors 초기화.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|WeaponCollision")
	void StopDetection();

	/**
	 * MeshComp(캐릭터 스켈레탈 메시) 소유 캐릭터의 장착 무기에서 본 컴포넌트를 찾아 반환하는 정적 헬퍼.
	 * 노티파이가 호출하며, 무기 미장착/컴포넌트 부재 시 nullptr + Warning 반환.
	 */
	static UBWWeaponCollisionComponent* FindOnEquippedWeapon(const USkeletalMeshComponent* MeshComp);

protected:
	virtual void BeginPlay() override;

	// ── BP 설정용 — 무기 BP 자식에서 값 지정 ──────────────────────────

	/** Sweep 시작 소켓명(칼날 밑동). 무기 SM에 추가된 소켓명과 일치시킨다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponCollision|Trace")
	FName TraceStartSocket = TEXT("trace_start");

	/** Sweep 끝 소켓명(칼끝). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponCollision|Trace")
	FName TraceEndSocket = TEXT("trace_end");

	/** Sweep 캡슐 반경(cm). 칼날 두께에 맞춰 BP에서 튜닝한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponCollision|Trace", meta = (ClampMin = "0.0"))
	float TraceRadius = 8.f;

	/** Sweep 충돌 채널. 적 캡슐이 반응하는 채널로 설정한다(기본 ECC_Pawn). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponCollision|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	/** true이면 매 Sweep 캡슐을 디버그 라인으로 그린다(개발 확인용). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponCollision|Debug")
	bool bDrawDebug = false;

	/** 데미지 타입 클래스(선택). 미지정 시 UDamageType 기본값 사용. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponCollision|Damage")
	TSubclassOf<UDamageType> DamageTypeClass;

private:
	/**
	 * 이전 프레임 Start/End → 현재 프레임 Start/End 보간 Sweep을 수행한다.
	 * 적중 액터를 AlreadyHitActors로 필터링한 후 ApplyDamageTo를 호출한다.
	 */
	void PerformSweep();

	/**
	 * 단일 피격 처리: ShotDirection 계산 → FPointDamageEvent 구성 → DamagedActor->TakeDamage 호출.
	 */
	void ApplyDamageTo(const FHitResult& Hit);

	/**
	 * 무기 루트 StaticMeshComponent의 소켓 월드 위치를 반환한다.
	 * 소켓이 없으면 액터 위치로 폴백 후 1회 Warning 출력.
	 */
	bool GetSocketWorldLocation(FName SocketName, FVector& OutLocation) const;

	// ── GC 추적 런타임 상태 ─────────────────────────────────────────────

	/** 현재 감지 윈도우에서 이미 타격한 액터 집합. 중복 타격 방지. GC 추적을 위해 UPROPERTY. */
	UPROPERTY()
	TSet<TObjectPtr<AActor>> AlreadyHitActors;

	// ── 원시 런타임 상태 (UObject 아님 — UPROPERTY 불필요) ─────────────

	/** 이전 프레임의 Sweep 시작 소켓 월드 위치. StartDetection에서 현재 위치로 시드. */
	FVector PrevStartLocation = FVector::ZeroVector;

	/** 이전 프레임의 Sweep 끝 소켓 월드 위치. StartDetection에서 현재 위치로 시드. */
	FVector PrevEndLocation = FVector::ZeroVector;

	/** 현재 감지 중인지 여부. */
	bool bDetecting = false;

	/** BeginPlay에서 캐시한 무기 루트 StaticMeshComponent. */
	TWeakObjectPtr<UStaticMeshComponent> CachedWeaponMesh;
};
