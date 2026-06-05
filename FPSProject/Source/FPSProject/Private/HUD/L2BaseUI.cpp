// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/L2BaseUI.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"


void UL2BaseUI::NativeConstruct()
{
	Super::NativeConstruct();
}




void UL2BaseUI::ItemSetting(int oil, int heal, int box)
{
	item1 = oil;
	item2 = heal;
	item3 = box;
	EngineOilText->SetText(FText::FromString(FString::FromInt(item1)));
	HealPackText->SetText(FText::FromString(FString::FromInt(item2)));
	ToolBoxText->SetText(FText::FromString(FString::FromInt(item3)));
}

void UL2BaseUI::UsingItem(int num)
{
	switch (num) {
	case 1:	//기름
		if (0 < item1) {
			item1--;
			EngineOilText->SetText(FText::FromString(FString::FromInt(item1)));
			//이제 기름바..회복..
		}
		break;
	case 2:	//힐팩
		if (0 < item2) {
			item2--;
			HealPackText->SetText(FText::FromString(FString::FromInt(item2)));
			//이제 체력회복하는 코드.
		}
		break;
	case 3: //정비툴박스
		if (0 < item3) {
			item3--;
			ToolBoxText->SetText(FText::FromString(FString::FromInt(item3)));
			//이제 차량회복 코드
		}
		break;
	}
}