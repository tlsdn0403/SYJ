// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/SlotWidget.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"  


void USlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void USlotWidget::SetItem(UTexture2D* IconTexture, int hand)
{
	if (!IconTexture || !ItemIcon)  return;

	ItemIcon->SetBrushFromTexture(IconTexture, false);

	if (hand >= 1) {
		ItemIcon->SetColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f));
	}
}

void USlotWidget::ClearSlot()
{
	if (!ItemIcon) return;
	ItemIcon->SetColorAndOpacity(FLinearColor::White);
	ItemIcon->SetBrushFromTexture(BaseTexture, false);

}

void USlotWidget::PlayAni_Select()
{
	if (Selected)
		PlayAnimation(SelectAni, 0.f, 0);
	else
		StopAnimation(SelectAni);
}

void USlotWidget::StopAni_Select()
{
	PlayAnimation(SelectAni);
}