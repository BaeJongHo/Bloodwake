// Fill out your copyright notice in the Description page of Project Settings.

#include "Equipment/BWArmour.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/BWAttributeComponent.h"

ABWArmour::ABWArmour()
{
	// 방어구는 이벤트 기반(장착/해제)으로 처리한다. 매 프레임 틱 비활성.
	PrimaryActorTick.bCanEverTick = false;

	// ArmourMesh: 방어구 외형 전용 스켈레탈 메시.
	// 베이스의 MeshComponent(StaticMesh, 루트)에 attach한다 — 무기 sweep 경로 무영향.
	// LeaderPose는 EquipItem에서 설정(생성자 시점엔 플레이어 메시 참조 불가).
	ArmourMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArmourMesh"));
	ArmourMesh->SetupAttachment(GetRootComponent());

	// 방어구는 외형만 담당한다. 콜리전·물리 시뮬레이션 비활성.
	ArmourMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArmourMesh->SetSimulatePhysics(false);
}

// ── ABWEquipItem 오버라이드 ─────────────────────────────────────────────────

USceneComponent* ABWArmour::GetAttachMeshComponent() const
{
	return ArmourMesh;
}

USkeletalMesh* ABWArmour::GetDisplaySkeletalMesh() const
{
	// 픽업 미리보기는 ArmourMesh의 스켈레탈 메시 에셋을 표시한다.
	// CDO에서 호출 시에도 BP 자식이 (상속됨) ArmourMesh에 지정한 SK_ 에셋을 읽는다.
	return ArmourMesh ? ArmourMesh->GetSkeletalMeshAsset() : nullptr;
}

// ── 장착 / 해제 ─────────────────────────────────────────────────────────────

void ABWArmour::EquipItem(USkeletalMeshComponent* OwnerMesh, UBWAttributeComponent* Attribute)
{
	if (!OwnerMesh)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BWArmour] EquipItem: OwnerMesh가 유효하지 않습니다. (%s)"), *GetName());
		return;
	}

	// 1. 이 액터의 루트(StaticMesh)를 OwnerMesh에 부착한다.
	//    NAME_None — 방어구는 특정 소켓이 아닌 메시 루트에 부착해 LeaderPose로 본을 따른다.
	AttachToComponent(
		OwnerMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		NAME_None
	);

	// 2. ArmourMesh가 플레이어 메시의 본 트랜스폼을 추종하도록 LeaderPose 설정.
	if (ArmourMesh)
	{
		ArmourMesh->SetLeaderPoseComponent(OwnerMesh);
	}

	// 3. 방어력 누적.
	if (Attribute)
	{
		Attribute->IncreaseDefense(DefenseAmount);
	}

	UE_LOG(LogTemp, Log,
		TEXT("[BWArmour] EquipItem: %s 부착 완료 (DefenseAmount=%.1f)"), *GetName(), DefenseAmount);
}

void ABWArmour::UnequipItem(UBWAttributeComponent* Attribute)
{
	// 1. LeaderPose 해제 — 댕글링 본 참조 방지.
	if (ArmourMesh)
	{
		ArmourMesh->SetLeaderPoseComponent(nullptr);
	}

	// 2. 부착 해제 — 드롭/파괴 전 월드 공간 유지.
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// 3. 방어력 차감.
	if (Attribute)
	{
		Attribute->DecreaseDefense(DefenseAmount);
	}

	UE_LOG(LogTemp, Log,
		TEXT("[BWArmour] UnequipItem: %s 해제 완료 (DefenseAmount=%.1f)"), *GetName(), DefenseAmount);
}
