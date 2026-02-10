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

UCLASS()
class FPSPROJECT_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;

public:

	// 슬롯들을 담을 박스
	UPROPERTY( meta = (BindWidget))
	UHorizontalBox* SlotBox;	//UHorizontalBox = UI 위젯들을 “가로로 자동 정렬해서 담아주는 컨테이너(상자)” 객체

	// 슬롯 위젯 배열
	UPROPERTY()
	TArray<USlotWidget*> SlotWidgets;

	void SetItem(int32 SlotIndex, UTexture2D* IconTexture);
};
