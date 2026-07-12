// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BWWeaponCollisionComponent.generated.h"

class UPrimitiveComponent;
class USkeletalMeshComponent;
class UDamageType;
class ABWWeapon;

/**
 * 캐릭터(UBWCombatComponent)가 소유하는 히트 판정 컴포넌트.
 * 감지 ON 동안(StartDetection~StopDetection) 매 틱 sweep 소스(무기 소켓 또는 손 본)의
 * Start~End 선분을 여러 지점으로 샘플링해 각 지점의 이전→현재 위치를 구체 sweep 하여
 * 피격 액터를 검출하고 TakeDamage(FPointDamageEvent)로 데미지를 전달한다.
 * (Start·End 두 끝점만 sweep하면 창·폴암처럼 소켓 간격이 큰 무기는 그 "사이" 구간이 비어 헛맞는다.)
 * 한 감지 윈도우 내 동일 액터 중복 타격을 방지(AlreadyHitActors 캐시).
 * 히트 윈도우 제어는 UBWAnimNotifyState_WeaponCollision이 담당한다.
 *
 * sweep 소스와 데미지 소스는 외부에서 주입한다(ConfigureForWeapon / ConfigureForFists / ClearSource).
 * 무기 전투: 무기 SM 소켓 사이 sweep, 데미지=무기 BaseDamage.
 * 맨손 전투: 캐릭터 SK 손 본 위치 sweep, 데미지=FistDamage.
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

	// ── 히트 윈도우 제어 ───────────────────────────────────────────────

	/**
	 * 히트 윈도우 시작. UBWAnimNotifyState_WeaponCollision::NotifyBegin에서 호출한다.
	 * AlreadyHitActors 초기화, PrevLocation 소켓/본 위치로 시드, Tick ON.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|WeaponCollision")
	void StartDetection();

	/**
	 * 히트 윈도우 종료. UBWAnimNotifyState_WeaponCollision::NotifyEnd에서 호출한다.
	 * Tick OFF, AlreadyHitActors 초기화.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|WeaponCollision")
	void StopDetection();

	// ── sweep 소스 주입 API ────────────────────────────────────────────

	/**
	 * sweep 대상을 무기 모드로 설정한다.
	 * InWeaponMesh의 InStart/InEnd 소켓 사이를 sweep하고, 데미지 소스로 InWeapon을 사용한다.
	 * UBWCombatComponent::RefreshCombatState가 무기 장착 시 호출한다.
	 */
	void ConfigureForWeapon(UPrimitiveComponent* InWeaponMesh, FName InStart, FName InEnd,
		float InRadius, const ABWWeapon* InWeapon);

	/**
	 * sweep 대상을 맨손 모드로 설정한다.
	 * 캐릭터 스켈레탈 메시의 손 본(InHandBone) 위치를 sweep하고,
	 * 데미지 소스로 InFistDamage를 사용한다.
	 * 손 본은 단일 지점이므로 Start==End=손 본 위치(구체 sweep)로 처리한다.
	 * UBWCombatComponent::RefreshCombatState가 맨손 전환 시 호출한다.
	 */
	void ConfigureForFists(USkeletalMeshComponent* InCharacterMesh, FName InHandBone,
		float InRadius, float InFistDamage);

	/**
	 * 현재 sweep 소스를 비활성(미설정)으로 만든다.
	 * 전투 타입 전환 직전에 호출해 잘못된 소스로 sweep하는 것을 방지한다.
	 */
	void ClearSource();

	// ── 디버그/채널 설정 (UPROPERTY — BP에서 조정 가능) ────────────────

	/** Sweep 충돌 채널. 적 캡슐이 반응하는 채널로 설정한다(기본 ECC_Pawn). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponCollision|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	/**
	 * 블레이드(Start~End)를 따라 sweep할 최대 샘플 지점 수(성능 상한).
	 * 실제 지점 수는 블레이드 길이 / 반경으로 계산되며, 이 값을 넘지 않도록 클램프된다.
	 * 긴 무기(창·폴암)일수록 지점이 많아지므로 상한으로 스윕 폭주를 방지한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponCollision|Trace", meta = (ClampMin = "2", ClampMax = "64"))
	int32 MaxSweepSamples = 24;

	/** true이면 매 Sweep 캡슐을 디버그 라인으로 그린다(개발 확인용). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponCollision|Debug")
	bool bDrawDebug = false;

	/** 데미지 타입 클래스(선택). 미지정 시 UDamageType 기본값 사용. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WeaponCollision|Damage")
	TSubclassOf<UDamageType> DamageTypeClass;

private:
	/**
	 * 이전 프레임 위치 → 현재 프레임 위치 보간 Sweep을 수행한다.
	 * 적중 액터를 AlreadyHitActors로 필터링한 후 ApplyDamageTo를 호출한다.
	 */
	void PerformSweep();

	/**
	 * 단일 피격 처리: ShotDirection 계산 → FPointDamageEvent 구성 → DamagedActor->TakeDamage 호출.
	 */
	void ApplyDamageTo(const FHitResult& Hit);

	/**
	 * 현재 설정된 소스(SourceMesh + SourceStartSocket)에서 시작 소켓 월드 위치를 반환한다.
	 * 소켓/본이 없으면 컴포넌트 위치로 폴백하고 false를 반환한다.
	 */
	bool GetStartSocketWorldLocation(FVector& OutLocation) const;

	/**
	 * 현재 설정된 소스(SourceMesh + SourceEndSocket)에서 끝 소켓 월드 위치를 반환한다.
	 * 맨손 모드(Start==End)에서는 GetStartSocketWorldLocation과 동일 위치를 반환한다.
	 */
	bool GetEndSocketWorldLocation(FVector& OutLocation) const;

	// ── GC 추적 런타임 상태 ─────────────────────────────────────────────

	/** 현재 감지 윈도우에서 이미 타격한 액터 집합. 중복 타격 방지. GC 추적을 위해 UPROPERTY. */
	UPROPERTY()
	TSet<TObjectPtr<AActor>> AlreadyHitActors;

	// ── sweep 소스 (외부 주입, TWeakObjectPtr) ──────────────────────────

	/**
	 * sweep 대상 메시. 무기 모드=UStaticMeshComponent(또는 UPrimitiveComponent),
	 * 맨손 모드=USkeletalMeshComponent. 둘 다 GetSocketLocation을 지원한다.
	 */
	TWeakObjectPtr<UPrimitiveComponent> SourceMesh;

	/** sweep 시작 소켓/본 이름. 무기=trace_start, 맨손=손 본 이름(hand_r/hand_l). */
	FName SourceStartSocket;

	/**
	 * sweep 끝 소켓/본 이름. 무기=trace_end, 맨손=손 본 이름(Start와 동일).
	 * 맨손은 단일 점 sweep이므로 Start==End.
	 */
	FName SourceEndSocket;

	/** Sweep 캡슐 반경(cm). ConfigureFor* 주입값으로 설정된다. */
	float SourceRadius = 8.f;

	// ── 데미지 소스 (외부 주입, TWeakObjectPtr / 원시값) ────────────────

	/**
	 * 무기 모드 데미지 소스. 유효하면 GetBaseDamage() 사용, null이면 FistDamage 사용.
	 * 소유 관계 없음(약참조). const: 데미지 조회만 하며 무기 상태를 변경하지 않는다.
	 */
	TWeakObjectPtr<const ABWWeapon> SourceWeapon;

	/** 맨손 모드 데미지. ConfigureForFists 주입값. SourceWeapon이 null일 때 사용. */
	float FistDamage = 0.f;

	/**
	 * 무시할 캐릭터(데미지 instigator/owner 판정용). ConfigureFor* 시점에 GetOwner()로 캐시.
	 * 약참조(소유 관계 없음).
	 */
	TWeakObjectPtr<AActor> OwnerCharacter;

	// ── 원시 런타임 상태 ──────────────────────────────────────────────────

	/** 이전 프레임의 Sweep 시작 위치. StartDetection에서 현재 위치로 시드. */
	FVector PrevStartLocation = FVector::ZeroVector;

	/** 이전 프레임의 Sweep 끝 위치. StartDetection에서 현재 위치로 시드. */
	FVector PrevEndLocation = FVector::ZeroVector;

	/** 현재 감지 중인지 여부. */
	bool bDetecting = false;

	/** sweep 소스가 유효하게 설정돼 있는지 여부. ClearSource 후 false. */
	bool bSourceConfigured = false;

	/**
	 * 소켓 미존재 경고를 이미 출력했는지 여부.
	 * ConfigureForWeapon / ConfigureForFists / ClearSource 시 false로 리셋된다.
	 * 매 틱 ensure 폭발을 막기 위해 첫 발생 시 1회만 Warning을 출력한다.
	 * const 메서드(GetStart/EndSocketWorldLocation)에서 갱신하므로 mutable로 선언한다.
	 */
	mutable bool bWarnedMissingSocket = false;
};
