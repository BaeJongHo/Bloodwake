// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BWEnemyHealthBarComponent.h"

#include "Blueprint/UserWidget.h"
#include "Combat/BWAttributeComponent.h"
#include "UI/BWEnemyHealthBarWidget.h"
#include "AI/BWAILog.h"

UBWEnemyHealthBarComponent::UBWEnemyHealthBarComponent()
{
	// Screen-space 모드: 카메라/거리에 무관하게 화면 좌표계로 렌더링된다.
	SetWidgetSpace(EWidgetSpace::Screen);

	// 위젯 크기를 컨텐츠 원하는 크기에 맞게 자동 설정한다.
	SetDrawAtDesiredSize(true);

	// HP 바는 충돌이 필요 없다.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 시작 시 숨김 — 플레이어에게 감지될 때 ShowBar()로 표시한다.
	SetVisibility(false);
	SetHiddenInGame(true);

	// Screen 위젯의 화면 레이어(SWorldWidgetScreenLayer) 등록/해제는
	// UWidgetComponent::TickComponent → UpdateWidget() 에서 수행된다.
	// Tick이 꺼져 있으면 HideBar 후에도 레이어에서 위젯이 제거되지 않아 화면에 남는다.
	// (UBWLockOnWidgetComponent 동일 선례: BWLockOnWidgetComponent.cpp:23-29 주석 참조)
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBWEnemyHealthBarComponent::BeginPlay()
{
	Super::BeginPlay();

	// 소유 액터에서 AttributeComponent를 1회 캐시한다(매 틱 탐색 금지 — CLAUDE.md 4.1).
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	UBWAttributeComponent* AttrComp = Owner->FindComponentByClass<UBWAttributeComponent>();
	if (!IsValid(AttrComp))
	{
		UE_LOG(LogBWAI, Warning,
			TEXT("[BWEnemyHealthBarComponent] 소유 액터(%s)에서 UBWAttributeComponent를 찾을 수 없습니다."),
			*Owner->GetName());
		return;
	}

	CachedAttributeComponent = AttrComp;

	// HP 변경 델리게이트 구독.
	AttrComp->OnHealthChanged.AddDynamic(this, &UBWEnemyHealthBarComponent::HandleHealthChanged);

	// 컴포넌트 BeginPlay 순서 비보장 대비 — 초기값 push.
	// AttributeComponent가 이미 유효한 MaxHealth로 초기화된 경우에만 현재값으로 즉시 동기화한다.
	// MaxHealth == 0이면 아직 AttrComp의 BeginPlay가 실행되지 않은 것이므로 push를 건너뛴다.
	// 이 경우 AttributeComponent가 초기화되면 OnHealthChanged Broadcast로 정상값이 전달된다.
	// (0으로 push하면 위젯이 일시적으로 HP 0%로 표시되는 문제 방지 — ue-reviewer [3])
	if (AttrComp->GetMaxHealth() > 0.f)
	{
		HandleHealthChanged(AttrComp->GetHealth(), AttrComp->GetMaxHealth());
	}
}

void UBWEnemyHealthBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 댕글링 콜백 방지 — 델리게이트 해제 (CLAUDE.md 4.3).
	if (CachedAttributeComponent.IsValid())
	{
		CachedAttributeComponent->OnHealthChanged.RemoveDynamic(
			this, &UBWEnemyHealthBarComponent::HandleHealthChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void UBWEnemyHealthBarComponent::ShowBar()
{
	// 컴포넌트 가시성: 다음 Tick에 화면 레이어로 등록되어 HP 바가 나타난다.
	SetVisibility(true);
	SetHiddenInGame(false);

	// 내부 UMG 위젯 가시성도 직접 켠다 — 화면 레이어 등록 타이밍과 무관하게 즉시 반영.
	if (UUserWidget* BarWidget = GetWidget())
	{
		BarWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UBWEnemyHealthBarComponent::HideBar()
{
	SetVisibility(false);
	SetHiddenInGame(true);

	// 내부 UMG 위젯을 직접 Collapse한다.
	// Screen 위젯은 SWorldWidgetScreenLayer가 컨테이너를 visible로 유지하므로
	// 컴포넌트 SetVisibility(false)만으로는 제거가 지연될 수 있다.
	// (UBWLockOnWidgetComponent::HideMarker 동일 패턴)
	if (UUserWidget* BarWidget = GetWidget())
	{
		BarWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UBWEnemyHealthBarComponent::HandleHealthChanged(float NewValue, float MaxValue)
{
	// GetWidget()을 UBWEnemyHealthBarWidget으로 캐스팅해 SetHealthPercent를 호출한다.
	// WidgetClass가 WBP_EnemyHealthBar(UBWEnemyHealthBarWidget 파생)로 지정되어야 캐스팅이 성공한다.
	UBWEnemyHealthBarWidget* HealthBarWidget = Cast<UBWEnemyHealthBarWidget>(GetWidget());
	if (!HealthBarWidget)
	{
		// WidgetClass 미지정 또는 BP 자식 미설정 상태 — 경고는 한 번만 출력해도 충분하나,
		// 여기서는 조용히 return한다(표시 불가 상태이므로 warning 남발 불필요).
		return;
	}

	const float Percent = (MaxValue > 0.f) ? (NewValue / MaxValue) : 0.f;
	HealthBarWidget->SetHealthPercent(Percent);
}
