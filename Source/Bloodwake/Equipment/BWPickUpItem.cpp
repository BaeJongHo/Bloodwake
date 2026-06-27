// Fill out your copyright notice in the Description page of Project Settings.

#include "Equipment/BWPickUpItem.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Equipment/BWEquipItem.h"

ABWPickUpItem::ABWPickUpItem()
{
	// 픽업 액터는 매 프레임 틱이 필요 없다. 이벤트 기반으로 처리한다.
	PrimaryActorTick.bCanEverTick = false;

	// 구체 콜리전이 루트 — 캐릭터의 SweepMultiByChannel 트레이스에 감지된다.
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->SetSphereRadius(50.f);

	// ECC_Visibility 채널에 Block 응답해 기본 트레이스 채널로 감지되게 한다.
	// 커스텀 채널이 필요하면 BP에서 응답을 변경한다.
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// 캐릭터 이동을 막지 않도록 Pawn 채널은 Ignore로 유지(기본값).

	// 픽업 표식 메시(StaticMesh 장비 — 무기·방패). EquipItemClass CDO 메시가 자동 적용된다.
	DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
	DisplayMesh->SetupAttachment(CollisionSphere);
	// 표식 메시는 물리/콜리전 없음 — 시각 표시 전용.
	DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DisplayMesh->SetSimulatePhysics(false);

	// 픽업 표식 메시(SkeletalMesh 장비 — 방어구). 애님 인스턴스 없이 레퍼런스 포즈로만 표시.
	DisplaySkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DisplaySkeletalMesh"));
	DisplaySkeletalMesh->SetupAttachment(CollisionSphere);
	DisplaySkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DisplaySkeletalMesh->SetSimulatePhysics(false);
	// 미리보기 표식은 애니메이션이 필요 없다 — 틱·애님 갱신 비용 제거.
	DisplaySkeletalMesh->SetComponentTickEnabled(false);
	DisplaySkeletalMesh->bNoSkeletonUpdate = true;
}

void ABWPickUpItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서 EquipItemClass를 변경할 때마다 DisplayMesh를 즉시 갱신한다(미리보기).
	ApplyDisplayMeshFromClass();
}

void ABWPickUpItem::BeginPlay()
{
	Super::BeginPlay();

	// 런타임(쿠킹 빌드)에서도 외형을 보장 적용한다(OnConstruction과의 일관성).
	ApplyDisplayMeshFromClass();
}

void ABWPickUpItem::InitializeFromEquip(TSubclassOf<ABWEquipItem> InEquipItemClass)
{
	// 드롭 재생성 시 CombatComponent가 스폰 후 호출하는 주입 경로.
	EquipItemClass = InEquipItemClass;
	ApplyDisplayMeshFromClass();
}

void ABWPickUpItem::Consume()
{
	// 1단계: 단순 파괴. 오브젝트 풀 반환은 후속 작업.
	Destroy();
}

void ABWPickUpItem::ApplyDisplayMeshFromClass()
{
	// EquipItemClass가 null이면 양쪽 미리보기를 비운다.
	if (!EquipItemClass)
	{
		ClearDisplayMeshes();
		return;
	}

	// CDO에서 메시를 읽어 적용한다. CDO는 항상 유효하나 메시가 없을 수 있으므로 null 폴백 필수.
	const ABWEquipItem* CDO = EquipItemClass.GetDefaultObject();
	if (!CDO)
	{
		ClearDisplayMeshes();
		return;
	}

	// 스켈레탈 메시(방어구) 우선 — 있으면 그쪽을 표시하고 StaticMesh 미리보기는 숨긴다.
	USkeletalMesh* SkeletalMesh = CDO->GetDisplaySkeletalMesh();
	if (SkeletalMesh)
	{
		if (DisplaySkeletalMesh)
		{
			DisplaySkeletalMesh->SetSkeletalMeshAsset(SkeletalMesh);
			DisplaySkeletalMesh->SetVisibility(true);
		}
		if (DisplayMesh)
		{
			DisplayMesh->SetStaticMesh(nullptr);
			DisplayMesh->SetVisibility(false);
		}
		return;
	}

	// 스켈레탈 메시가 없으면 StaticMesh(무기·방패)를 표시하고 스켈레탈 미리보기는 숨긴다.
	if (DisplayMesh)
	{
		// Mesh가 nullptr이어도 SetStaticMesh(nullptr)로 안전 폴백됨.
		DisplayMesh->SetStaticMesh(CDO->GetDisplayStaticMesh());
		DisplayMesh->SetVisibility(true);
	}
	if (DisplaySkeletalMesh)
	{
		DisplaySkeletalMesh->SetSkeletalMeshAsset(nullptr);
		DisplaySkeletalMesh->SetVisibility(false);
	}
}

void ABWPickUpItem::ClearDisplayMeshes()
{
	if (DisplayMesh)
	{
		DisplayMesh->SetStaticMesh(nullptr);
		DisplayMesh->SetVisibility(false);
	}
	if (DisplaySkeletalMesh)
	{
		DisplaySkeletalMesh->SetSkeletalMeshAsset(nullptr);
		DisplaySkeletalMesh->SetVisibility(false);
	}
}
