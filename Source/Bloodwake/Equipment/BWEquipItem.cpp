// Fill out your copyright notice in the Description page of Project Settings.

#include "Equipment/BWEquipItem.h"

#include "Components/StaticMeshComponent.h"

ABWEquipItem::ABWEquipItem()
{
	// 전투 장비는 이벤트 기반으로 처리한다. 매 프레임 틱 비활성.
	PrimaryActorTick.bCanEverTick = false;

	// 루트 메시 컴포넌트 생성. BP 자식에서 SM_ 에셋을 지정한다.
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	// 기본 충돌 비활성 — 스폰 후 SetEquippedPhysicsState가 명시적으로 호출될 때까지 비활성 유지.
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetSimulatePhysics(false);
}

void ABWEquipItem::AttachToCharacter(USceneComponent* AttachTarget, FName SocketName)
{
	if (!AttachTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BWEquipItem] AttachToCharacter: AttachTarget이 유효하지 않습니다. (%s)"), *GetName());
		return;
	}

	AttachToComponent(
		AttachTarget,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName
	);
}

void ABWEquipItem::MoveToHolster(USceneComponent* AttachTarget, FName HolsterSocketName)
{
	AttachToCharacter(AttachTarget, HolsterSocketName);
}

void ABWEquipItem::MoveToHand(USceneComponent* AttachTarget, FName HandSocketName)
{
	AttachToCharacter(AttachTarget, HandSocketName);
}

UStaticMesh* ABWEquipItem::GetDisplayStaticMesh() const
{
	if (!MeshComponent)
	{
		return nullptr;
	}

	return MeshComponent->GetStaticMesh();
}

void ABWEquipItem::SetEquippedPhysicsState()
{
	if (!MeshComponent)
	{
		return;
	}

	// 캐릭터에 부착된 장비는 충돌·물리 시뮬레이션을 끈다.
	MeshComponent->SetSimulatePhysics(false);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
