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