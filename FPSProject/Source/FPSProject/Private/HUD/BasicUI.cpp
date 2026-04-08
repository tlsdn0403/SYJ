// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/BasicUI.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"


void UBasicUI::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBasicUI::GetGunAR4() {

	if (GunImage) {
		GunImage->SetRenderOpacity(1.f);
		GunImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 1.f));
		GunImage->SetBrushTintColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 1.f)));
		GunText->SetRenderOpacity(1.f);
		GunText->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 1.f));
	}
}

void UBasicUI::SetHealth(float CurrentHP,float MaxHP) {

	if (!HpBar) return;

	float Ratio = (float)CurrentHP / MaxHP;

	FLinearColor HpColor;

	if (Ratio > 0.6f)
	{
		HpColor = FLinearColor(0.06f, 0.406f, 0.04f, 1.f); // 초록
	}
	else if (Ratio > 0.3f)
	{
		HpColor = FLinearColor(1.f, 0.235f, 0.07f, 1.f); // 주황
	}
	else
	{
		HpColor = FLinearColor(0.443f, 0.047f, 0.044f, 1.f); // 빨강
	}
	//if (Ratio > 0.6f)
	//{
	//	HpBar->SetFillColorAndOpacity(FLinearColor::Green);
	//}
	//else if (Ratio > 0.3f)
	//{
	//	HpBar->SetFillColorAndOpacity(FLinearColor(1.f, 0.5f, 0.f, 1.f)); // 주황
	//}
	//else
	//{
	//	HpBar->SetFillColorAndOpacity(FLinearColor::Red);
	//}
	//if (CurrentHP < 80 && CurrentHP >= 50) { //주황색 셋팅
	//	HpText->SetColorAndOpacity(FLinearColor(1.f, 0.5f, 0.f, 1.f));
	//}
	//else if (CurrentHP < 50) { //빨간색 셋팅
	//	HpText->SetColorAndOpacity(FLinearColor(1.f, 0.f, 0.f, 1.f)); // 빨강
	//}
	//else { //초록색 셋팅
	//	HpText->SetColorAndOpacity(FLinearColor(0.f, 1.f, 0.f, 1.f)); // 초록
	//}

	HpBar->SetFillColorAndOpacity(HpColor);
	HpText->SetColorAndOpacity(HpColor);
	HpBar->SetPercent(Ratio);
	HpText->SetText(FText::FromString(FString::FromInt(CurrentHP)));
}