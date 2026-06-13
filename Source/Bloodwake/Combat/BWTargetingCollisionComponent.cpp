// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWTargetingCollisionComponent.h"

UBWTargetingCollisionComponent::UBWTargetingCollisionComponent()
{
	// Tick 비활성 — 정적 형상이므로 매 프레임 갱신 불필요.
	PrimaryComponentTick.bCanEverTick = false;

	// 기본 반경. BP 자식의 "(상속됨)" 컴포넌트에서 가슴 높이에 맞게 조정한다.
	InitSphereRadius(60.f);

	// Query 전용으로 설정해 물리 시뮬레이션 비용을 피한다.
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// 모든 채널을 Ignore로 초기화한 후 Targeting 채널에만 Block 응답.
	// ObjectType은 WorldDynamic으로 두어 기본 프로파일 충돌을 방지한다.
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
}
