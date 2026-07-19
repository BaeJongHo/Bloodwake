// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/BWEnemy_Boss.h"

#include "Audio/BWBossMusicSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "UI/BWBossHealthBarWidget.h"
#include "UI/BWEnemyHealthBarComponent.h"
#include "Combat/BWAttributeComponent.h"
#include "Combat/BWCombatComponent.h"
#include "Equipment/BWEquipItem.h"

ABWEnemy_Boss::ABWEnemy_Boss()
{
	// Tick은 부모(ABWEnemy) 생성자에서 이미 비활성화된다.
	// 생성자에서는 DefaultSubobject 생성 외 월드 의존 작업을 하지 않는다.
	// 오버헤드 HP 바 제거 및 2D 위젯 배선은 BeginPlay에서 수행한다.
}

void ABWEnemy_Boss::BeginPlay()
{
	// Super::BeginPlay()를 먼저 호출해 무기 스폰 및 OnDeath 구독이 완료된 후
	// 오버헤드 HP 바를 파괴한다. Super 이후 파괴해야 컴포넌트 EndPlay에서
	// Attribute 델리게이트가 안전하게 해제되므로 델리게이트 누수가 없다.
	Super::BeginPlay();

	// 오버헤드 HP 바 컴포넌트 제거 — 보스는 2D 스크린 바를 사용한다.
	// DestroyComponent로 제거하면 해당 컴포넌트의 EndPlay가 호출되어 자체 RemoveDynamic을 처리한다.
	if (IsValid(HealthBarWidget))
	{
		HealthBarWidget->DestroyComponent();
		HealthBarWidget = nullptr;
	}
}

void ABWEnemy_Boss::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 2D 보스 체력바를 뷰포트에서 제거한다.
	// NativeDestruct가 Attribute OnHealthChanged 구독을 해제한다(댕글링 콜백 방지 — CLAUDE.md 4.3).
	if (IsValid(BossHealthBarInstance))
	{
		BossHealthBarInstance->RemoveFromParent();
		BossHealthBarInstance = nullptr;
	}

	// PIE 종료·레벨 전환 시 BGM을 즉시 정지한다(페이드 없이 Stop — Deinitialize보다 먼저 처리).
	// UBWBossMusicSubsystem::Deinitialize도 Stop을 호출하지만, EndPlay 시점에 먼저 정리하면
	// PIE 재시작 시 오디오 컴포넌트 댕글링을 방지할 수 있다.
	StopBossBGM();

	Super::EndPlay(EndPlayReason);
}

void ABWEnemy_Boss::ShowHealthBar()
{
	// 최초 노출 시 위젯을 생성하고 뷰포트에 추가한다.
	EnsureBossHealthBar();

	if (IsValid(BossHealthBarInstance))
	{
		// HitTestInvisible: 화면에 표시되지만 마우스 클릭을 통과시킨다(전투 UI 비간섭).
		BossHealthBarInstance->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// 보스와 전투 돌입 시 BGM을 재생한다.
	StartBossBGM();
}

void ABWEnemy_Boss::HideHealthBar()
{
	// 위젯이 없으면(아직 ShowHealthBar가 호출되지 않은 경우) 아무것도 하지 않는다.
	if (IsValid(BossHealthBarInstance))
	{
		// Collapsed: 파괴하지 않고 숨김 — 시야 복귀 시 재노출 가능.
		BossHealthBarInstance->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 시야를 잃으면 BGM을 정지한다.
	StopBossBGM();
}

void ABWEnemy_Boss::EnableRagdoll()
{
	// 1) 사망 직전 무기를 손에서 떨구어 물리 낙하시킨다.
	DropEquippedWeapon();

	// 2) 2D 보스 체력바를 뷰포트에서 제거한다.
	if (IsValid(BossHealthBarInstance))
	{
		BossHealthBarInstance->RemoveFromParent();
		BossHealthBarInstance = nullptr;
	}

	// 3) 사망 시 BGM을 정지한다 — Super::EnableRagdoll() 전에 호출해 페이드아웃이 시작되도록 한다.
	StopBossBGM();

	// 4) 부모 랙돌 처리 (bIsDead·Death 태그·랙돌·CorpseLifeSpan 등 공통 사망 처리).
	//    부모 EnableRagdoll의 HealthBarWidget.HideBar()는 BeginPlay에서 파괴(nullptr)되었으므로
	//    IsValid(HealthBarWidget) == false → 안전하게 무동작.
	Super::EnableRagdoll();
}

void ABWEnemy_Boss::DropEquippedWeapon()
{
	if (!IsValid(CombatComponent))
	{
		return;
	}

	ABWEquipItem* Weapon = CombatComponent->GetEquippedWeapon();
	if (!IsValid(Weapon))
	{
		// 무기 없음 — 맨손 보스이거나 이미 드롭됨. 조용히 무시.
		return;
	}

	// 캐릭터에서 detach 후 물리 시뮬레이션 활성화.
	// SM 에셋에 Simple Collision(바디 셋업)이 필요하다(에셋 작업).
	Weapon->DropFromCharacter(DroppedWeaponCollisionProfile);

	// 잔존 시간 설정 (0이면 영구 잔존, 보스 시체와 독립적).
	if (DroppedWeaponLifeSpan > 0.f)
	{
		Weapon->SetLifeSpan(DroppedWeaponLifeSpan);
	}
}

void ABWEnemy_Boss::StartBossBGM()
{
	if (!IsValid(BossBGM))
	{
		// BP_Boss에서 BossBGM 에셋을 지정하지 않았으면 조용히 무시한다.
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UBWBossMusicSubsystem* MusicSubsystem = World->GetSubsystem<UBWBossMusicSubsystem>();
	if (!IsValid(MusicSubsystem))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ABWEnemy_Boss] StartBossBGM: UBWBossMusicSubsystem을 찾을 수 없습니다. (%s)"), *GetName());
		return;
	}

	MusicSubsystem->PlayBossMusic(BossBGM, this, BGMFadeInDuration);
}

void ABWEnemy_Boss::StopBossBGM()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UBWBossMusicSubsystem* MusicSubsystem = World->GetSubsystem<UBWBossMusicSubsystem>();
	if (!IsValid(MusicSubsystem))
	{
		// 서브시스템이 없으면(예: 이미 월드 해제 중) 조용히 무시한다.
		return;
	}

	MusicSubsystem->StopBossMusic(this, BGMFadeOutDuration);
}

void ABWEnemy_Boss::EnsureBossHealthBar()
{
	// 이미 유효한 인스턴스가 있으면 재생성하지 않는다.
	if (IsValid(BossHealthBarInstance))
	{
		return;
	}

	// 위젯 클래스 null 가드 — BP_Boss에서 설정 필요.
	if (!BossHealthBarWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ABWEnemy_Boss] EnsureBossHealthBar: BossHealthBarWidgetClass가 설정되지 않았습니다. (%s)"),
			*GetName());
		return;
	}

	// PlayerController null 가드 — 월드가 없거나 플레이어가 없을 때 안전.
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PC))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ABWEnemy_Boss] EnsureBossHealthBar: PlayerController를 찾을 수 없습니다. (%s)"),
			*GetName());
		return;
	}

	// 위젯 생성 및 뷰포트 추가.
	// CreateWidget은 PC를 소유자로 삼아 위젯 수명을 PlayerController에 묶는다.
	BossHealthBarInstance = CreateWidget<UBWBossHealthBarWidget>(PC, BossHealthBarWidgetClass);
	if (!IsValid(BossHealthBarInstance))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ABWEnemy_Boss] EnsureBossHealthBar: CreateWidget 실패. (%s)"),
			*GetName());
		return;
	}

	BossHealthBarInstance->AddToViewport();

	// AttributeComponent와 보스 이름을 위젯에 전달해 구독 및 초기값을 설정한다.
	BossHealthBarInstance->InitializeForBoss(AttributeComponent, BossName);
}
