// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BWPotion.generated.h"

class UStaticMeshComponent;

/**
 * 포션 마시기 몽타주 진행 중 손 소켓에 붙는 순수 외형 액터.
 * 로직 없음 — 메시 표시 전용.
 * SM_ 에셋은 BP_Potion(BP 자식)의 (상속됨) Mesh 컴포넌트에서 지정한다.
 */
UCLASS(Blueprintable)
class BLOODWAKE_API ABWPotion : public AActor
{
	GENERATED_BODY()

public:
	ABWPotion();

protected:
	/** 포션 외형 스태틱 메시. 루트 컴포넌트. 충돌 비활성(연출 전용). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Potion")
	TObjectPtr<UStaticMeshComponent> Mesh;
};
