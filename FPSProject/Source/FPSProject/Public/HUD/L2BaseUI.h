// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "L2BaseUI.generated.h"

/**
 *
 */

class UImage;
class UTextBlock;

UCLASS()
class FPSPROJECT_API UL2BaseUI : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	int item1 = 0;
	int item2 = 0;
	int item3 = 0;
public:

	UPROPERTY(meta = (BindWidget))	//
		UImage* HealPack;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* HealPackText;


	UPROPERTY(meta = (BindWidget))	//
		UImage* ToolBox;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ToolBoxText;

	UPROPERTY(meta = (BindWidget))	//
		UImage* EngineOil;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* EngineOilText;

	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* Ani_ItemOpen;

	UPROPERTY(meta = (BindWidget))		//아이템 부족합니다! 멘트
	UTextBlock* errorT;

	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* Ani_itemError;
	
	void ItemSetting(int oil, int heal, int box);
	bool UsingItem(int num);

};
