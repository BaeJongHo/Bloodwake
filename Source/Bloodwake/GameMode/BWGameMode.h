// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BWGameMode.generated.h"

class UBWMainHUDWidget;

/**
 * Bloodwake 기본 게임 모드.
 * BeginPlay에서 플레이어의 AttributeComponent를 찾아 메인 HUD 위젯을 생성하고 뷰포트에 추가한다.
 * BP 자식(BP_GameMode)에서 MainHUDWidgetClass에 WBP_MainHUD를 지정해야 HUD가 표시된다.
 */
UCLASS()
class BLOODWAKE_API ABWGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABWGameMode();

protected:
	virtual void BeginPlay() override;

	/**
	 * HUD 초기화 실제 작업.
	 * BeginPlay에서 직접 호출하지 않고 SetTimerForNextTick으로 한 틱 뒤에 실행한다.
	 * 이렇게 해야 Character/AttributeComponent의 BeginPlay가 모두 끝난 뒤
	 * Attribute 초기값(MaxHealth 등)을 읽을 수 있어 HUD 0% 깜빡임이 발생하지 않는다.
	 */
	void InitializeHUD();

protected:
	/**
	 * 생성할 메인 HUD 위젯 클래스.
	 * BP 자식(BP_GameMode)의 디폴트에서 WBP_MainHUD(부모=UBWMainHUDWidget)를 지정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UBWMainHUDWidget> MainHUDWidgetClass;

	/** 생성된 메인 HUD 위젯 인스턴스. */
	UPROPERTY(Transient)
	TObjectPtr<UBWMainHUDWidget> MainHUDWidget;
};
