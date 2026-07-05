// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BWPotionWidget.generated.h"

class UTextBlock;

/**
 * 포션 수량 텍스트 표시 위젯의 C++ 베이스 클래스.
 * 레이아웃·스타일은 WBP_Potion(BP 자식)에서 구성하며,
 * 동일 이름(PotionQuantityText)의 TextBlock 위젯을 반드시 배치해야 한다(BindWidget 매칭).
 */
UCLASS(Blueprintable)
class BLOODWAKE_API UBWPotionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 포션 수량 텍스트를 갱신한다.
	 * PotionQuantityText->SetText(FText::AsNumber(InAmount)).
	 * PotionQuantityText가 유효하지 않으면 조용히 건너뛴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetPotionQuantity(const int32 InAmount);

private:
	/** WBP_Potion 자식에서 동일 이름 "PotionQuantityText" TextBlock을 배치해야 한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PotionQuantityText;
};
