// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/InventoryWidget.h"

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

void UInventoryWidget:: PlayAin_Slot(int32 SlotIndex)
{
	//USlotWidget* TargetSlotWidget = nullptr;
 //   switch (SlotIndex) {
 //       case 0:
	//		TargetSlotWidget = SlotWidgets[0];
	//	break;
	//	case 1:
	//		TargetSlotWidget = SlotWidgets[1];
	//		break;
	//	case 2:     
	//		TargetSlotWidget = SlotWidgets[2];
	//		break;
	//	case 3:
	//		TargetSlotWidget = SlotWidgets[3];
	//		break;
	//	case 4:
	//		TargetSlotWidget = SlotWidgets[4];
	//		break;
 //   }
 //   if (TargetSlotWidget)
 //   {
 //       TargetSlotWidget->PlayAni_Select();
 //   }

	if (SlotWidgets.IsValidIndex(SlotIndex) && IsValid(SlotWidgets[SlotIndex]))
	{
		SlotWidgets[SlotIndex]->PlayAni_Select();
	}
}

//void UInventoryWidget::SetItem(int32 Index, UTexture2D* IconTexture)
//{
//    if (!SlotWidgets.IsValidIndex(Index)) return;
//
//   // SlotWidgets[Index]->SetItem(IconTexture);
//}

void UInventoryWidget::SelectSlot(int32 slotnum)
{
	for (int32 i = 0; i < SlotWidgets.Num(); ++i)
	{
		if (SlotWidgets.IsValidIndex(i) && IsValid(SlotWidgets[i]))
		{
			if (i == slotnum)
			{
				SlotWidgets[i]->Selected= !SlotWidgets[i]->Selected;
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