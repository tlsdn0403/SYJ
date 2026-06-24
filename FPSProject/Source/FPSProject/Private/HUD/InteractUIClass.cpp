// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/InteractUIClass.h"
#include "Components/TextBlock.h"
void UInteractUIClass::NativeConstruct()
{
	Super::NativeConstruct();
	InteractText->SetIsEnabled(true);
}
void UInteractUIClass::PlayAni_PopUp(bool bOpen)
{
	if (!PopUp) return;
	PlayAnimation(PopUp);

}

void UInteractUIClass::RePlayAni_PopUp()
{
	if (PopUp)
	{
		PlayAnimation(PopUp, 0.f, 1, EUMGSequencePlayMode::Reverse);
	}
}

void UInteractUIClass::SetInteractText(const FText& NewText)
{
	if (InteractText)
	{
		InteractText->SetText(NewText);
	}
}

void UInteractUIClass::SetDoorInteractText(const bool bOpen)
{
	InteractText->SetText(FText::FromString(bOpen ? TEXT("Close") : TEXT("Open")));
}