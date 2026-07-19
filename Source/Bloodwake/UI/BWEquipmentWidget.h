// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWEquipmentWidget.generated.h"

class UImage;
class UTexture2D;

/**
 * 장비 아이콘 1칸의 표시 위젯의 C++ 베이스 클래스.
 * 텍스처를 받아 UImage 브러시에 적용한다. 로직 없음(순수 표시). Tick 없음.
 *
 * WBP_Equipment 자식 설정 필수:
 *  - 이름이 정확히 "EquipmentIcon"인 Image 위젯을 배치해야 한다(BindWidget 매칭).
 *    이름이 다르면 WBP 컴파일 에러 발생(의도된 안전장치).
 */
UCLASS(Blueprintable)
class BLOODWAKE_API UBWEquipmentWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 장비 아이콘 텍스처를 적용한다.
	 * InIcon이 유효하면 SetBrushFromTexture(InIcon, false) + Visible.
	 * InIcon이 null이면 ClearEquipmentIcon()을 호출해 아이콘을 비우고 Hidden으로 전환한다(빈 슬롯).
	 * EquipmentIcon이 유효하지 않으면 조용히 건너뛴다(UBWPotionWidget::SetPotionQuantity 계약과 동일).
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD|Equipment")
	void SetEquipmentIcon(UTexture2D* InIcon);

	/** 아이콘을 비우고 슬롯을 숨긴다. SetEquipmentIcon(nullptr)의 의미 명시 래퍼. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Equipment")
	void ClearEquipmentIcon();

private:
	/**
	 * WBP_Equipment 자식에서 동일 이름 "EquipmentIcon" UImage를 반드시 배치해야 한다(BindWidget 매칭).
	 * GC 추적: UWidget 파생이므로 TObjectPtr + UPROPERTY 필수(CLAUDE.md 3.3).
	 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> EquipmentIcon;
};
