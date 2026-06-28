// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/BasicUI.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Characters/FPSPlayerController.h"
#include "FPSProjectGameInstance.h"

void UBasicUI::NativeConstruct()
{
	Super::NativeConstruct();
	if (ReB)
	{
		ReB->OnClicked.AddDynamic(this, &UBasicUI::OnResumeClicked);
	}

	if (EndB)
	{
		EndB->OnClicked.AddDynamic(this, &UBasicUI::OnExitClicked);
	}

	if (NameText)
	{
		if (const UFPSProjectGameInstance* GameInstance = GetGameInstance<UFPSProjectGameInstance>())
		{
			NameText->SetText(FText::FromString(GameInstance->GetPlayerNickname()));
		}
	}
}

void UBasicUI::GetGunAR4() {

	if (GunImage) {
		GunImage->SetRenderOpacity(1.f);
		GunImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 1.f));
		GunImage->SetBrushTintColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 1.f)));
	}

	if (GunText) {
		GunText->SetRenderOpacity(1.f);
		GunText->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 1.f));
	}

	if (GunMaxText) {
		GunMaxText->SetRenderOpacity(1.f);
		GunMaxText->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 1.f));
	}
}

void UBasicUI::SetHealth(float CurrentHP,float MaxHP) {

	if (!HpBar) return;

	float Ratio = FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f);

	FLinearColor HpColor;

	if (Ratio > 0.6f)
	{
		HpColor = FLinearColor(0.06f, 0.406f, 0.04f, 0.5f); // 초록
	}
	else if (Ratio > 0.3f)
	{
		HpColor = FLinearColor(1.f, 0.235f, 0.07f, 0.5f); // 주황
	}
	else
	{
		HpColor = FLinearColor(0.443f, 0.047f, 0.044f, 0.5f); // 빨강
	}

	HpBar->SetFillColorAndOpacity(HpColor);
	HpColor.A = 1.0f; // 텍스트는 투명도 1로 고정`
	HpText->SetColorAndOpacity(HpColor);
	HpBar->SetPercent(Ratio);
	HpText->SetText(FText::FromString(FString::FromInt(CurrentHP)));
}


void UBasicUI::SetAmmoCount(int32 AmmoBoxCount)
{
	constexpr int32 AmmoBulletsPerBoxForUI = 40;
	const int32 BulletCount = FMath::Max(AmmoBoxCount, 0) * AmmoBulletsPerBoxForUI;

	if (GunText)
	{
		GunText->SetText(FText::FromString(FString::FromInt(BulletCount)));
	}

	if (GunMaxText)
	{
		GunMaxText->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), BulletCount)));
	}
}

void UBasicUI::SetRemainingAmmoCount(int32 BulletCount)
{
	if (GunText)
	{
		GunText->SetText(FText::FromString(FString::FromInt(FMath::Max(BulletCount, 0))));
	}
}
void UBasicUI:: Play_ESC()
{ 
	RemoveFromParent();
	AddToViewport(1000);

	if (Ani_ESC)
	{
		PlayAnimation(Ani_ESC);
	}
	if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetOwningPlayer()))
	{
		PC->bShowMouseCursor = true;

		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}
}

void UBasicUI::OnResumeClicked()
{

	if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetOwningPlayer()))
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}

void UBasicUI::OnExitClicked()
{
	PlayAnimationReverse(Ani_ESC);

	if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetOwningPlayer()))
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = false;
	}
}

