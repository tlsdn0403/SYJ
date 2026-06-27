// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/L2BaseUI.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/RadialSlider.h"

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

bool UL2BaseUI::UsingItem(int num)
{
	bool success = false;
	switch (num) {
	case 1:	//기름
		if (0 < item1) {
			item1--;
			success = true;
			EngineOilText->SetText(FText::FromString(FString::FromInt(item1)));
			//이제 기름바..회복..
		}
		break;
	case 2:	//힐팩
		if (0 < item2) {
			item2--;
			success = true;
			HealPackText->SetText(FText::FromString(FString::FromInt(item2)));
			//이제 체력회복하는 코드.
		}
		break;
	case 3: //정비툴박스
		if (0 < item3) {
			item3--;
			success = true;
			ToolBoxText->SetText(FText::FromString(FString::FromInt(item3)));
			//이제 차량회복 코드
		}
		break;
	}
	return success;
}


void UL2BaseUI::OilUpdate(float CurrentFuel, float MaxFuel)
{
	const float FuelRatio = MaxFuel > KINDA_SMALL_NUMBER
		? FMath::Clamp(CurrentFuel / MaxFuel, 0.0f, 1.0f)
		: 0.0f;

	if (OilSlider)
	{
		OilSlider->SetValue(FuelRatio);

		if (FuelRatio <= 0.2f)
		{
			OilSlider->SetSliderProgressColor(FLinearColor(1.0f, 0.1f, 0.0f, 1.0f));
			PlayAnimation(Ani_Oil_Icon);
		}
		else if (FuelRatio <= 0.4f)
		{
			OilSlider->SetSliderProgressColor(FLinearColor(1.0f, 0.45f, 0.0f, 1.0f));
			PlayAnimation(Ani_Oil_Icon);
		}
		else if (FuelRatio <= 0.6f)
		{
			OilSlider->SetSliderProgressColor(FLinearColor(1.0f, 0.55f, 0.0f, 1.0f));
		}
		else
		{
			OilSlider->SetSliderProgressColor(FLinearColor(0.2f, 0.85f, 0.25f, 1.0f));
		}
	}
}
