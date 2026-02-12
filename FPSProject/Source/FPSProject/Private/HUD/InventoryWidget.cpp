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
	USlotWidget* TargetSlotWidget = nullptr;
    switch (SlotIndex) {
        case 0:
			TargetSlotWidget = SlotWidgets[0];
		break;
		case 1:
			TargetSlotWidget = SlotWidgets[1];
			break;
		case 2:     
			TargetSlotWidget = SlotWidgets[2];
			break;
		case 3:
			TargetSlotWidget = SlotWidgets[3];
			break;
		case 4:
			TargetSlotWidget = SlotWidgets[4];
			break;
    }
    if (TargetSlotWidget)
    {
        TargetSlotWidget->PlayAni_Select();
    }
}

//void UInventoryWidget::SetItem(int32 Index, UTexture2D* IconTexture)
//{
//    if (!SlotWidgets.IsValidIndex(Index)) return;
//
//   // SlotWidgets[Index]->SetItem(IconTexture);
//}