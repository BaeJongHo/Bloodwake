// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "BWTargetingCollisionComponent.generated.h"

/**
 * 락온 스윕이 맞히는 구체 형상 컴포넌트.
 * Targeting 채널(ECC_GameTraceChannel1)에 Block 응답하도록 설정된다.
 * 반경/위치는 BP 자식(BP_Enemy)의 "(상속됨)" 컴포넌트에서 튜닝한다.
 * 적/보스에 부착한다.
 */
UCLASS(ClassGroup = (Bloodwake), meta = (BlueprintSpawnableComponent))
class BLOODWAKE_API UBWTargetingCollisionComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	UBWTargetingCollisionComponent();
};
