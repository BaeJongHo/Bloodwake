// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BWPotionWidget.h"

#include "Components/TextBlock.h"

void UBWPotionWidget::SetPotionQuantity(const int32 InAmount)
{
	if (!IsValid(PotionQuantityText))
	{
		return;
	}

	PotionQuantityText->SetText(FText::AsNumber(InAmount));
}
