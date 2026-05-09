// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryWidget.generated.h"

/**
 * 
 */

class USlotWidget;
class UHorizontalBox;
class UImage;
class UTextBlock;


UCLASS()
class FPSPROJECT_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;

public:

	// 슬롯들을 담을 박스 =======================================================================================
	UPROPERTY(meta = (BindWidget))	//
	UHorizontalBox* SlotBox;	//UHorizontalBox = UI 위젯들을 “가로로 자동 정렬해서 담아주는 컨테이너(상자)” 객체

	// 슬롯 위젯 배열
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<USlotWidget> SlotWidgetClass;
	
	UPROPERTY(meta = (BindWidget))
	USlotWidget* Slot1;

	UPROPERTY(meta = (BindWidget))
	USlotWidget* Slot2;

	UPROPERTY(meta = (BindWidget))
	USlotWidget* Slot3;

	UPROPERTY(meta = (BindWidget))
	USlotWidget* Slot4;

	UPROPERTY(meta = (BindWidget))
	USlotWidget* Slot5;

	UPROPERTY()
	TArray<USlotWidget*> SlotWidgets;

	void SelectSlot(int32 slotnum);	// 슬롯 선택시 깜빡이는 효과
	void PlayAin_Slot(int32 SlotIndex);
	/*UPROPERTY()
	USlotWidget* SlotW;*/

	//void SetItem(int32 SlotIndex, UTexture2D* IconTexture);
	void PickUp_Item(UTexture2D* image, int32 handw);
};
