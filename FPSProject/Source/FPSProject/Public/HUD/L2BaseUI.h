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
class URadialSlider;

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

	//===================================주유파트
	UPROPERTY(meta = (BindWidget))	//기름바 
	URadialSlider* OilSlider;

	UPROPERTY(meta = (BindWidget))	// 기름 부족시 깜빡임
	UImage* Oil_Icon;

	UPROPERTY(meta = (BindWidgetAnim), Transient)	//깜빡임애니
	UWidgetAnimation* Ani_Oil_Icon;
	
	void ItemSetting(int oil, int heal, int box);
	bool UsingItem(int num);

};
