// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BWTargetingInterface.generated.h"

class UBWTargetingCollisionComponent;
class UBWLockOnWidgetComponent;

/**
 * 락온 대상이 될 수 있는 액터(적/보스)가 구현해야 하는 순수 C++ 인터페이스.
 * 플레이어는 구현하지 않는다.
 * 로직은 전부 C++에서 처리하므로 BlueprintNativeEvent 미사용.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UBWTargetingInterface : public UInterface
{
	GENERATED_BODY()
};

class BLOODWAKE_API IBWTargetingInterface
{
	GENERATED_BODY()

public:
	/**
	 * 현재 타깃 가능한 상태인지(살아있고 락온 허용).
	 * 구현부에서 StateComponent->HasStateTag(Character_State_Death)로 판정한다.
	 */
	virtual bool CanBeTargeted() const = 0;

	/** 락온 후보 수집/소켓 기준점이 될 타깃 콜리전 컴포넌트. */
	virtual UBWTargetingCollisionComponent* GetTargetingCollisionComponent() const = 0;

	/** 락온 마커 위젯 컴포넌트(on/off 토글 대상). */
	virtual UBWLockOnWidgetComponent* GetLockOnWidgetComponent() const = 0;

	/**
	 * 카메라가 바라볼/마커가 붙을 월드 위치(가슴/머리 소켓 기준).
	 * 기본 구현은 콜리전 컴포넌트 위치 반환; 서브클래스에서 오버라이드 가능.
	 */
	virtual FVector GetTargetFocusLocation() const = 0;
};
