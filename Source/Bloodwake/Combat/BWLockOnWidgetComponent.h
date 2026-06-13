// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "BWLockOnWidgetComponent.generated.h"

/**
 * Screen-space 락온 마커 위젯 컴포넌트.
 * WidgetClass는 BP 자식(BP_Enemy)의 "(상속됨)" 컴포넌트에서 WBP_LockOn으로 지정한다.
 * 현재 타깃에만 ShowMarker()를 호출하고, 해제/사망 시 HideMarker()를 호출한다.
 * 적/보스에 부착한다.
 */
UCLASS(ClassGroup = (Bloodwake), meta = (BlueprintSpawnableComponent))
class BLOODWAKE_API UBWLockOnWidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UBWLockOnWidgetComponent();

	/** 마커를 화면에 표시한다. 타깃 선택 시 호출된다. */
	UFUNCTION(BlueprintCallable, Category = "Combat|LockOn")
	void ShowMarker();

	/** 마커를 숨긴다. 타깃 해제/사망 시 호출된다. */
	UFUNCTION(BlueprintCallable, Category = "Combat|LockOn")
	void HideMarker();
};
