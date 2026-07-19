// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/BWEquipmentWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UBWEquipmentWidget::SetEquipmentIcon(UTexture2D* InIcon)
{
	if (!IsValid(EquipmentIcon))
	{
		return;
	}

	if (InIcon)
	{
		EquipmentIcon->SetBrushFromTexture(InIcon, /*bMatchSize=*/false);
		EquipmentIcon->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		ClearEquipmentIcon();
	}
}

void UBWEquipmentWidget::ClearEquipmentIcon()
{
	if (!IsValid(EquipmentIcon))
	{
		return;
	}

	EquipmentIcon->SetBrushFromTexture(nullptr, /*bMatchSize=*/false);
	EquipmentIcon->SetVisibility(ESlateVisibility::Hidden);
}
