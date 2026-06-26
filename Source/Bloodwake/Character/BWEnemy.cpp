// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/BWEnemy.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/DamageEvents.h"
#include "Engine/DataTable.h"
#include "Particles/ParticleSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"
#include "Combat/BWAttributeComponent.h"
#include "Combat/BWCombatComponent.h"
#include "Combat/BWAttackComponent.h"
#include "Combat/BWAttackTypes.h"
#include "Combat/BWTargetingCollisionComponent.h"
#include "Combat/BWLockOnWidgetComponent.h"
#include "Character/BWStateComponent.h"
#include "Core/BWGameplayDefine.h"
#include "Equipment/BWWeapon.h"
#include "UI/BWEnemyHealthBarComponent.h"
#include "Perception/AISense_Damage.h"

ABWEnemy::ABWEnemy()
{
	// 적은 이벤트/입력 기반이므로 매 프레임 틱 비활성.
	PrimaryActorTick.bCanEverTick = false;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// AttributeComponent — 체력·스태미나·기력 관리 (플레이어와 동일 패턴)
	AttributeComponent = CreateDefaultSubobject<UBWAttributeComponent>(TEXT("AttributeComponent"));

	// StateComponent — 행동 상태(GameplayTag) 관리
	StateComponent = CreateDefaultSubobject<UBWStateComponent>(TEXT("StateComponent"));

	// CombatComponent — 장비 보유·장착·해제 로직 (플레이어와 동일 컴포넌트 재사용)
	CombatComponent = CreateDefaultSubobject<UBWCombatComponent>(TEXT("CombatComponent"));

	// AttackComponent — 공격(콤보) 로직 (플레이어와 동일 컴포넌트 재사용)
	AttackComponent = CreateDefaultSubobject<UBWAttackComponent>(TEXT("AttackComponent"));

	// TargetingCollision — 락온 스윕이 맞히는 구체 형상. 루트에 부착(메시 기준 높이는 BP에서 조정).
	TargetingCollision = CreateDefaultSubobject<UBWTargetingCollisionComponent>(TEXT("TargetingCollision"));
	TargetingCollision->SetupAttachment(RootComponent);

	// LockOnWidget — Screen-space 락온 마커. 기본 숨김, 타깃 시 ShowMarker 호출.
	LockOnWidget = CreateDefaultSubobject<UBWLockOnWidgetComponent>(TEXT("LockOnWidget"));
	LockOnWidget->SetupAttachment(RootComponent);

	// HealthBarWidget — 머리 위 Screen-space HP 바. 기본 숨김, 감지 시 ShowBar 호출.
	// Widget Class(WBP_EnemyHealthBar)는 BP 자식의 "(상속됨)" 컴포넌트에서 지정한다.
	HealthBarWidget = CreateDefaultSubobject<UBWEnemyHealthBarComponent>(TEXT("HealthBarWidget"));
	HealthBarWidget->SetupAttachment(RootComponent);
}

void ABWEnemy::BeginPlay()
{
	Super::BeginPlay();

	// 사망 델리게이트 구독. 체력 0 도달 시 HandleDeath 호출.
	if (AttributeComponent)
	{
		AttributeComponent->OnDeath.AddDynamic(this, &ABWEnemy::HandleDeath);
	}

	// DefaultWeaponClass가 지정된 경우 무기를 스폰해 손에 즉시 장착한다.
	SpawnAndEquipDefaultWeapon();
}

void ABWEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 댕글링 콜백 방지 — 델리게이트 해제 (규약 4.3)
	if (AttributeComponent)
	{
		AttributeComponent->OnDeath.RemoveDynamic(this, &ABWEnemy::HandleDeath);
	}

	Super::EndPlay(EndPlayReason);
}

// ── IBWTargetingInterface 구현 ────────────────────────────────────────────────

bool ABWEnemy::CanBeTargeted() const
{
	// 사망 플래그 또는 Death 상태 태그가 있으면 타깃 불가.
	if (bIsDead)
	{
		return false;
	}

	if (IsValid(StateComponent) && StateComponent->HasStateTag(BWGameplayTags::Character_State_Death.GetTag()))
	{
		return false;
	}

	return true;
}

UBWTargetingCollisionComponent* ABWEnemy::GetTargetingCollisionComponent() const
{
	return TargetingCollision;
}

UBWLockOnWidgetComponent* ABWEnemy::GetLockOnWidgetComponent() const
{
	return LockOnWidget;
}

FVector ABWEnemy::GetTargetFocusLocation() const
{
	if (IsValid(TargetingCollision))
	{
		return TargetingCollision->GetComponentLocation();
	}

	// 폴백: 액터 위치에서 캡슐 절반 높이만큼 올린 가슴 높이 근사값
	return GetActorLocation() + FVector(0.f, 0.f, GetCapsuleComponent() ?
		GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.6f : 80.f);
}

// ── IBWCombatInterface 구현 ───────────────────────────────────────────────────

void ABWEnemy::PerformAttack(FOnMontageEnded OnEnded)
{
	// 1) Dead 가드 — 사망한 적은 공격 불가
	if (bIsDead)
	{
		OnEnded.ExecuteIfBound(nullptr, true);
		return;
	}

	// 2) Hit 상태 가드 — 피격 경직 중에는 공격 불가
	if (IsValid(StateComponent) && StateComponent->HasStateTag(BWGameplayTags::Character_State_Hit.GetTag()))
	{
		OnEnded.ExecuteIfBound(nullptr, true);
		return;
	}

	// 3) DataTable 유효성 검사
	if (!EnemyAttackDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWEnemy] PerformAttack: EnemyAttackDataTable이 설정되지 않았습니다. (%s)"), *GetName());
		OnEnded.ExecuteIfBound(nullptr, true);
		return;
	}

	// 4) DataTable에서 모든 행의 모든 Step을 풀로 모아 랜덤 선택
	//    공격 DataTable의 모든 행(Light/Running/Special/Heavy)에서 Step을 수집한다.
	//    Equip/Unequip 행은 스태미나 소모 없이 재생하면 안 되므로 제외한다.
	TArray<FBWComboStep> AllSteps;

	const TArray<FName>& RowNames = EnemyAttackDataTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		// Equip/Unequip 행 제외
		if (RowName == TEXT("Equip") || RowName == TEXT("Unequip"))
		{
			continue;
		}

		const FBWAttackComboRow* Row = EnemyAttackDataTable->FindRow<FBWAttackComboRow>(RowName, TEXT("BWEnemy::PerformAttack"));
		if (Row)
		{
			for (const FBWComboStep& Step : Row->Steps)
			{
				if (Step.Montage)
				{
					AllSteps.Add(Step);
				}
			}
		}
	}

	if (AllSteps.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWEnemy] PerformAttack: EnemyAttackDataTable에 재생 가능한 몽타주 Step이 없습니다. (%s)"), *GetName());
		OnEnded.ExecuteIfBound(nullptr, true);
		return;
	}

	// 랜덤 Step 선택
	const int32 RandIndex = FMath::RandRange(0, AllSteps.Num() - 1);
	const FBWComboStep& SelectedStep = AllSteps[RandIndex];

	// 5) 스태미나 체크 — 부족하면 OnEnded 즉시 호출로 BT Latent 교착 방지
	if (IsValid(AttributeComponent) && !AttributeComponent->HasEnoughStamina(SelectedStep.StaminaCost))
	{
		UE_LOG(LogTemp, Log, TEXT("[BWEnemy] PerformAttack: 스태미나 부족으로 공격 취소. (%s)"), *GetName());
		OnEnded.ExecuteIfBound(nullptr, true);
		return;
	}

	// 6) AnimInstance 획득
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWEnemy] PerformAttack: AnimInstance를 찾을 수 없습니다. (%s)"), *GetName());
		OnEnded.ExecuteIfBound(nullptr, true);
		return;
	}

	// 7) 공격 상태 태그 부착 (Character_Attack_Light 재사용 — 적 단순 공격 표시용)
	if (IsValid(StateComponent))
	{
		StateComponent->AddStateTag(BWGameplayTags::Character_Attack_Light.GetTag());
	}

	// 8) 몽타주 재생
	UAnimMontage* Montage = SelectedStep.Montage.Get();
	const float MontageLength = AnimInstance->Montage_Play(Montage);

	if (MontageLength <= 0.f)
	{
		// 재생 실패 — 태그 즉시 해제하고 BT Latent 종료
		UE_LOG(LogTemp, Warning, TEXT("[BWEnemy] PerformAttack: Montage_Play 실패. (%s)"), *GetName());
		if (IsValid(StateComponent))
		{
			StateComponent->RemoveStateTag(BWGameplayTags::Character_Attack_Light.GetTag());
		}
		OnEnded.ExecuteIfBound(Montage, true);
		return;
	}

	// 9) 스태미나 소비 — 몽타주 재생 성공 확인 후에 소비(재생 실패 시 누수 방지, BWAttackComponent 패턴 동일)
	if (IsValid(AttributeComponent))
	{
		AttributeComponent->ConsumeStamina(SelectedStep.StaminaCost);
	}

	// 10) ActiveAttackMontage 캐시 (AbortTask에서 정지에 사용)
	ActiveAttackMontage = Montage;

	// 11) 종료 델리게이트 바인딩 — BT가 넘긴 콜백을 그대로 AnimInstance에 전달
	AnimInstance->Montage_SetEndDelegate(OnEnded, Montage);
}

// ── TakeDamage 오버라이드 ─────────────────────────────────────────────────────

float ABWEnemy::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// 이미 사망한 적에게 추가 타격을 무시한다.
	if (bIsDead)
	{
		return 0.f;
	}

	// 엔진 표준 처리를 통해 실제 데미지 수치를 획득한다.
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// FPointDamageEvent에서 ImpactPoint/ShotDirection 추출
	FVector ImpactPoint = GetActorLocation();
	FVector ShotDirection = GetActorForwardVector(); // 폴백: 정면에서 맞은 것으로 처리

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		ImpactPoint = PointEvent.HitInfo.ImpactPoint;

		if (!PointEvent.ShotDirection.IsNearlyZero())
		{
			ShotDirection = PointEvent.ShotDirection;
		}
	}

	// 데미지를 AttributeComponent에 위임
	if (AttributeComponent)
	{
		AttributeComponent->ApplyDamage(ActualDamage);
	}

	// 피격 이펙트·리액션 재생 (사망은 OnDeath 델리게이트가 별도 트리거)
	PlayHitEffects(ImpactPoint);
	PlayHitReaction(ShotDirection);

	// AI Damage 감각 보고 — 시야 밖 피격 시에도 공격자 위치를 Perception에 인지시킨다.
	// Instigator가 있어야 Perception이 자극원을 올바르게 기록한다.
	if (IsValid(EventInstigator))
	{
		// 공격자 위치: DamageCauser(무기 등)가 있으면 그 위치, 없으면 적 본인 위치를 폴백으로 사용.
		const FVector EventLocation = IsValid(DamageCauser)
			? DamageCauser->GetActorLocation()
			: GetActorLocation();

		// Instigator Pawn(플레이어 캐릭터)을 자극원으로 보고한다.
		// GetPawn()이 null인 경우(예: AIController가 직접 Instigator인 경우)는 없지만 null 가드.
		AActor* InstigatorActor = IsValid(EventInstigator->GetPawn())
			? static_cast<AActor*>(EventInstigator->GetPawn())
			: nullptr;

		if (IsValid(InstigatorActor))
		{
			UAISense_Damage::ReportDamageEvent(
				this,               // WorldContextObject
				this,               // DamagedActor (이 적)
				InstigatorActor,    // Instigator (공격자 Pawn)
				ActualDamage,       // Amount
				EventLocation,      // EventLocation (공격자/무기 위치)
				GetActorLocation()  // HitLocation (피격 지점)
			);
		}
	}

	return ActualDamage;
}

// ── HandleDeath ───────────────────────────────────────────────────────────────

void ABWEnemy::HandleDeath()
{
	// bIsDead 가드 — HandleDeath 재진입 방지
	if (bIsDead)
	{
		return;
	}

	EnableRagdoll();
}

// ── HP 바 표시/숨김 API ────────────────────────────────────────────────────────

void ABWEnemy::ShowHealthBar()
{
	if (IsValid(HealthBarWidget))
	{
		HealthBarWidget->ShowBar();
	}
}

void ABWEnemy::HideHealthBar()
{
	if (IsValid(HealthBarWidget))
	{
		HealthBarWidget->HideBar();
	}
}

// ── private 구현부 ────────────────────────────────────────────────────────────

void ABWEnemy::AbortCurrentAttack()
{
	// 진행 중 공격 몽타주 정지
	if (IsValid(ActiveAttackMontage))
	{
		UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		if (AnimInstance)
		{
			AnimInstance->Montage_Stop(0.1f, ActiveAttackMontage.Get());
		}
		ActiveAttackMontage = nullptr;
	}

	// 공격 상태 태그 초기화
	if (IsValid(StateComponent))
	{
		StateComponent->RemoveStateTag(BWGameplayTags::Character_Attack_Light.GetTag());
	}
}

void ABWEnemy::SpawnAndEquipDefaultWeapon()
{
	if (!DefaultWeaponClass)
	{
		// DefaultWeaponClass 미설정 — 맨손으로 시작. CombatComponent의 RefreshCombatState가 FistAttackDataTable을 설정한다.
		return;
	}

	if (!IsValid(CombatComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWEnemy] SpawnAndEquipDefaultWeapon: CombatComponent가 유효하지 않습니다. (%s)"), *GetName());
		return;
	}

	// CombatComponent를 통해 무기를 장착한다.
	// EquipNewItem은 무기를 등(Back)에 부착하고 RefreshCombatState를 호출해 AttackDataTable/콜리전을 자동 배선한다.
	// 이후 AttachSlotToSocket(Weapon, Hand)로 손 소켓으로 즉시 이동시킨다.
	const bool bSuccess = CombatComponent->EquipNewItem(DefaultWeaponClass, FTransform::Identity);
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWEnemy] SpawnAndEquipDefaultWeapon: EquipNewItem 실패. (%s)"), *GetName());
		return;
	}

	// 적은 항상 무기를 손에 들고 시작한다 — 등→손 즉시 이동 (몽타주 없이 스냅).
	CombatComponent->AttachSlotToSocket(EBWEquipSlot::Weapon, EBWAttachDestination::Hand);

	UE_LOG(LogTemp, Log, TEXT("[BWEnemy] SpawnAndEquipDefaultWeapon: 무기를 손에 장착 완료. (%s)"), *GetName());
}

EBWHitDirection ABWEnemy::ComputeHitDirection(const FVector& ShotDirection) const
{
	// ShotDirection이 거의 0이면 기본값(Front) 반환
	if (ShotDirection.IsNearlyZero())
	{
		return EBWHitDirection::Front;
	}

	// 피격자 → 공격자 방향 (수평 평면 투영)
	FVector FromAttacker = -ShotDirection;
	FromAttacker.Z = 0.f;
	if (!FromAttacker.Normalize())
	{
		return EBWHitDirection::Front;
	}

	FVector Fwd = GetActorForwardVector();
	Fwd.Z = 0.f;
	Fwd.Normalize();

	FVector Right = GetActorRightVector();
	Right.Z = 0.f;
	Right.Normalize();

	const float ForwardDot = FVector::DotProduct(Fwd, FromAttacker);
	const float RightDot   = FVector::DotProduct(Right, FromAttacker);

	// COS45 ≈ 0.707 — 앞뒤를 ±45° 콘으로, 나머지를 좌우로 분류
	constexpr float COS45 = 0.707f;

	if (ForwardDot >= COS45)
	{
		return EBWHitDirection::Front;
	}
	else if (ForwardDot <= -COS45)
	{
		return EBWHitDirection::Back;
	}
	else if (RightDot > 0.f)
	{
		return EBWHitDirection::Right;
	}
	else
	{
		return EBWHitDirection::Left;
	}
}

void ABWEnemy::PlayHitReaction(const FVector& ShotDirection)
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	const EBWHitDirection Direction = ComputeHitDirection(ShotDirection);

	UAnimMontage* MontageToPlay = nullptr;
	switch (Direction)
	{
	case EBWHitDirection::Front: MontageToPlay = HitReactFrontMontage; break;
	case EBWHitDirection::Back:  MontageToPlay = HitReactBackMontage;  break;
	case EBWHitDirection::Left:  MontageToPlay = HitReactLeftMontage;  break;
	case EBWHitDirection::Right: MontageToPlay = HitReactRightMontage; break;
	}

	// 해당 방향 몽타주가 없으면 Front로 폴백
	if (!MontageToPlay)
	{
		MontageToPlay = HitReactFrontMontage;
	}

	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWEnemy] PlayHitReaction: 히트 리액션 몽타주가 설정되지 않았습니다. (%s)"),
			*GetName());
		return;
	}

	AnimInstance->Montage_Play(MontageToPlay);
}

void ABWEnemy::PlayHitEffects(const FVector& ImpactPoint)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Cascade 파티클 VFX 스폰
	if (HitVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			World,
			HitVFX,
			ImpactPoint,
			FRotator::ZeroRotator,
			/*bAutoDestroy=*/true
		);
	}

	// 사운드 재생
	if (HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, HitSound, ImpactPoint);
	}
}

void ABWEnemy::EnableRagdoll()
{
	// 1) 사망 플래그 설정 (이후 TakeDamage 재진입 차단)
	bIsDead = true;

	// 2) 진행 중인 행동 상태 태그 모두 해제 후 Death 태그 부착.
	//    ClearAllStateTags 이후 Death를 부착해야 CanBeTargeted()가 사망을 올바르게 감지한다.
	if (StateComponent)
	{
		StateComponent->ClearAllStateTags();
		StateComponent->AddStateTag(BWGameplayTags::Character_State_Death.GetTag());
	}

	// 락온 마커 숨기기 — 사망한 적에게 마커가 남지 않도록.
	// TargetingComponent의 ValidateCurrentTarget이 CanBeTargeted==false로 자동 EndLockOn을 하지만,
	// 동일 프레임 타이밍 차이로 한 프레임 마커가 보일 수 있어 여기서도 명시적으로 숨긴다.
	if (IsValid(LockOnWidget))
	{
		LockOnWidget->HideMarker();
	}

	// HP 바 숨기기 — 사망한 적 위에 HP 바가 남지 않도록 명시적으로 숨긴다.
	if (IsValid(HealthBarWidget))
	{
		HealthBarWidget->HideBar();
	}

	// 사망 시 Targeting 콜리전 비활성화 — 죽은 적이 재탐색 후보에 포함되지 않도록.
	if (IsValid(TargetingCollision))
	{
		TargetingCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 3) 진행 중인 몽타주 정지 (히트 리액션이 랙돌과 충돌하지 않도록)
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		AnimInstance->Montage_Stop(0.1f);
	}

	// 4) 이동 컴포넌트 비활성화
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->DisableMovement();
		MoveComp->StopMovementImmediately();
	}

	// 5) 캡슐 콜리전 OFF (랙돌 중 캡슐 충돌 제거)
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 6) 메시 물리 ON — 랙돌 전환
	//    콜리전 프로파일 설정 → 충돌 활성화 → 물리 시뮬레이션 시작 순서 준수
	if (USkeletalMeshComponent* EnemyMesh = GetMesh())
	{
		EnemyMesh->SetCollisionProfileName(RagdollCollisionProfile);
		EnemyMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		EnemyMesh->SetSimulatePhysics(true);
	}

	// 7) 시체 자동 제거 (CorpseLifeSpan > 0 인 경우만)
	if (CorpseLifeSpan > 0.f)
	{
		SetLifeSpan(CorpseLifeSpan);
	}
}
