// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/InventoryWidget.h"

#include "HUD/SlotWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

void UInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SlotWidgets.Empty();

  /*  InventoryWidget = Cast<UInventoryWidget>(CreateWidget(GetWorld(), InventoryClass));
    InventoryWidget->AddToViewport();*/

	if (!SlotWidgetClass) return;

   /* SlotW = Cast<USlotWidget>(CreateWidget(GetWorld(), SlotWidgetClass));
    SlotBox->AddChildToHorizontalBox(SlotW);*/
    // 5Ä­ »ý¼º
    for (int i = 0; i < 5; i++)
    {
        USlotWidget* Slot = Cast<USlotWidget>(CreateWidget(GetWorld(), SlotWidgetClass));

        if (Slot)
        {
            SlotBox->AddChildToHorizontalBox(Slot);
            SlotWidgets.Add(Slot);
        }
    }
}

//void UInventoryWidget::SetItem(int32 Index, UTexture2D* IconTexture)
//{
//    if (!SlotWidgets.IsValidIndex(Index)) return;
//
//   // SlotWidgets[Index]->SetItem(IconTexture);
//}