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

	// Screen 모드 위젯은 매 프레임 TickComponent에서 화면 좌표 레이어(SWorldWidgetScreenLayer)에
	// 자신을 등록하고 스크린 위치를 갱신한다. 따라서 Tick이 꺼져 있으면 위젯이 화면에 렌더되지 않는다.
	// 다만 마커가 보이는 동안에만 Tick을 켜서 평소 비용을 0으로 둔다(CLAUDE.md 4.1 규약).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UBWLockOnWidgetComponent::ShowMarker()
{
	SetVisibility(true);
	SetHiddenInGame(false);

	// Screen 좌표 레이어 등록/위치 갱신을 위해 Tick을 켠다.
	SetComponentTickEnabled(true);
}

void UBWLockOnWidgetComponent::HideMarker()
{
	SetVisibility(false);
	SetHiddenInGame(true);

	// 숨김 동안에는 Tick 불필요 — 비활성화로 비용 절약.
	SetComponentTickEnabled(false);
}
