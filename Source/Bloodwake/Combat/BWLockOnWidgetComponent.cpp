// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWLockOnWidgetComponent.h"

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
	// SetVisibility가 true가 되면 다음 Tick에 화면 레이어로 등록되어 마커가 나타난다.
	SetVisibility(true);
	SetHiddenInGame(false);
}

void UBWLockOnWidgetComponent::HideMarker()
{
	// SetVisibility가 false가 되면 다음 Tick에 화면 레이어에서 제거되어 마커가 사라진다.
	SetVisibility(false);
	SetHiddenInGame(true);
}
