// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWLockOnWidgetComponent.h"

#include "Blueprint/UserWidget.h"

UBWLockOnWidgetComponent::UBWLockOnWidgetComponent()
{
	// Screen-space 모드: 위젯이 항상 화면 좌표계로 렌더링된다.
	// 카메라/거리에 무관하게 고정 크기로 표시되어 소울라이크 락온 마커에 적합하다.
	SetWidgetSpace(EWidgetSpace::Screen);

	// 위젯 크기를 컨텐츠 원하는 크기에 맞게 자동 설정한다.
	SetDrawAtDesiredSize(true);

	// 락온 마커는 충돌이 필요 없다.
	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 시작 시 숨김 — ShowMarker/HideMarker로 토글한다.
	SetVisibility(false);
	SetHiddenInGame(true);

	// Screen 위젯의 화면 레이어(SWorldWidgetScreenLayer) "등록/해제"는 UWidgetComponent::TickComponent
	// → UpdateWidget() → UpdateWidgetOnScreen()에서 SetVisibility 상태를 보고 수행된다.
	// 따라서 표시/숨김이 반영되려면 컴포넌트 Tick이 살아 있어야 한다. Tick을 끄면 HideMarker가
	// 레이어에서 위젯을 제거하지 못해 이전 타깃 마커가 화면에 남는다(타깃 전환 시 마커 누적 버그).
	// 위젯의 매 프레임 화면 위치 갱신은 화면 레이어가 자체 Tick으로 처리하므로 이 Tick은 가볍다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBWLockOnWidgetComponent::ShowMarker()
{
	// 컴포넌트 가시성: 다음 Tick에 화면 레이어로 등록되어 마커가 나타난다.
	SetVisibility(true);
	SetHiddenInGame(false);

	// 내부 UMG 위젯 가시성도 직접 켠다 — 화면 레이어 등록 타이밍과 무관하게 즉시 반영.
	if (UUserWidget* MarkerWidget = GetWidget())
	{
		MarkerWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UBWLockOnWidgetComponent::HideMarker()
{
	SetVisibility(false);
	SetHiddenInGame(true);

	// 핵심: 내부 UMG 위젯을 직접 Collapse한다.
	// Screen 위젯은 SWorldWidgetScreenLayer가 컨테이너를 visible로 유지하므로(기본 cvar),
	// 컴포넌트 SetVisibility(false)만으로는 화면 레이어 제거가 한 프레임 지연되거나 누락될 수 있다.
	// 내부 위젯을 Collapse하면 컨테이너가 남아 있어도 내용물이 접혀 즉시 확정적으로 사라진다.
	if (UUserWidget* MarkerWidget = GetWidget())
	{
		MarkerWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}
