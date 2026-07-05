// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/BWPotion.h"

#include "Components/StaticMeshComponent.h"

ABWPotion::ABWPotion()
{
	// 포션은 연출 전용 — 매 프레임 틱 비활성.
	PrimaryActorTick.bCanEverTick = false;

	// 루트 스태틱 메시 생성. SM_ 에셋은 BP_Potion 자식에서 지정한다.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// 손에 붙는 연출 전용 — 충돌 불필요(BWEquipItem.cpp:16 패턴).
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetSimulatePhysics(false);
}
