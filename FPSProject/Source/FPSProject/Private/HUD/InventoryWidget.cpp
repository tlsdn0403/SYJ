// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/InventoryWidget.h"
#include "Components/Image.h"
#include "HUD/SlotWidget.h"
#include "Components/TextBlock.h"
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

void UInventoryWidget::PickUp_Item(UTexture2D* image,int32 handw) {

	int32 EmptyCount = 0;

	//필요한만큼 슬롯 남아있나 확인
	for (int32 i = 0; i < SlotWidgets.Num(); ++i)
	{
		if (IsValid(SlotWidgets[i]) && !SlotWidgets[i]->Selected)
		{
			EmptyCount++;

			if (EmptyCount >= handw)
			{
				break; // 필요한 만큼 빈 슬롯 찾았으니 더 볼 필요 없음
			}
		}
	}
	if (EmptyCount < handw)
	{
		UE_LOG(LogTemp, Warning, TEXT("빈 슬롯 부족"));
		return;
	}

	//어떤 손에 들어갈지 확인
	for(int i = 0,hand=0; i < SlotWidgets.Num(); ++i)
	{
		if (IsValid(SlotWidgets[i]) && !SlotWidgets[i]->Selected)
		{
			//아이템 아이콘 설정
			SlotWidgets[i]->Selected = true;
			SlotWidgets[i]->SetItem(image,hand);
			hand++;
			if(hand ==handw) break;
		}
	}
	return;
}

//void UInventoryWidget::SetItem(int32 Index, UTexture2D* IconTexture)
//{
//	if (!SlotWidgets.IsValidIndex(Index)) return;
//
//	SlotWidgets[Index]->SetItem(IconTexture);
//}

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
