// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/SlotWidget.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"  

//위젯은 엑터가 아님. 위젯에는 BeginPlay 같은 개념이 없음. 그래서 NativeConstruct() 사용

void USlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void USlotWidget::SetItem(UTexture2D* IconTexture)
{
	if (!IconTexture || !ItemIcon)  return;

	ItemIcon->SetBrushFromTexture(IconTexture, true); //true: 이미지 크기를 텍스처 크기에 맞춤
	//false: 텍스처를 이미지 크기에 맞춤

}

void USlotWidget::ClearSlot()
{
	if (!ItemIcon) return;

	ItemIcon->SetBrushFromTexture(BaseTexture, true);

}