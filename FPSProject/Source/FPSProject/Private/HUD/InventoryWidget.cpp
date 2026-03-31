// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/InventoryWidget.h"
#include "Components/Image.h"
#include "HUD/SlotWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

void UInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!SlotWidgetClass) return;



	SlotWidgets =
	{
		Slot1,
		Slot2,
		Slot3,
		Slot4,
		Slot5
	};
}

void UInventoryWidget::PlayAin_Slot(int32 SlotIndex)
{
	if (SlotWidgets.IsValidIndex(SlotIndex) && IsValid(SlotWidgets[SlotIndex]))
	{
		SlotWidgets[SlotIndex]->PlayAni_Select();
	}
}

void UInventoryWidget::PickUp_Item(UTexture2D* image) {
	//아이템 줍기
	//아이템에서 줍기 실행 시-> 이거 실행되게끔.. 

	//어떤 손에 들어갈지 확인
	for(int i = 0; i < SlotWidgets.Num(); ++i)
	{
		if (SlotWidgets.IsValidIndex(i) && IsValid(SlotWidgets[i]) && !SlotWidgets[i]->Selected)
		{

			//아이템 아이콘 설정
			SlotWidgets[i]->Selected = true;
			SlotWidgets[i]->SetItem(image);
			//아이템 차지 슬롯 몇개인지 확인하고 카운트 증가.
			//아이템이 다른 슬롯 차지해서 그런 경우 아이템 색 회색으로 그려지게끔...
			break;
		}
	}
	return;
}

void UInventoryWidget::SetItem(int32 Index, UTexture2D* IconTexture)
{
	if (!SlotWidgets.IsValidIndex(Index)) return;

	SlotWidgets[Index]->SetItem(IconTexture);
}

void UInventoryWidget::SelectSlot(int32 slotnum)
{
	for (int32 i = 0; i < SlotWidgets.Num(); ++i)
	{
		if (SlotWidgets.IsValidIndex(i) && IsValid(SlotWidgets[i]))
		{
			if (i == slotnum)
			{
				SlotWidgets[i]->Selected = !SlotWidgets[i]->Selected;
				//SlotWidgets[i]->PlayAni_Select();
			}
			else
			{
				SlotWidgets[i]->Selected = false;
				//SlotWidgets[i]->ClearSlot();
			}
			SlotWidgets[i]->PlayAni_Select();
		}
	}
}

void UInventoryWidget::GetGunAR4() {

	if (GunImage) {
		GunImage->SetRenderOpacity(1.f);
		GunImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 1.f));
		GunImage->SetBrushTintColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 1.f)));
	}
}