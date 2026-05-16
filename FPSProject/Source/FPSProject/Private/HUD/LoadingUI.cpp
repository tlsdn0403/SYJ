// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/LoadingUI.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "TimerManager.h"


void ULoadingUI::NativeConstruct()
{
	Super::NativeConstruct();
	UE_LOG(LogTemp, Warning, TEXT("REAL SLOT CREATED %p"), this);
	Players.Empty();

	Players.Add(Player1);
	Players.Add(Player2);
	Players.Add(Player3);
	Players[0]->SetColorAndOpacity(FLinearColor(0.74f, 0.74f, 0.74f, 1.f));
	for (int i = 1; i < 3; ++i) {

		Players[i]->SetColorAndOpacity(FLinearColor(0.0625f, 0.0625f, 0.0625f, 1.f));
	}

	DotCount = 0;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			LoadingDotTimerHandle,
			this,
			&ULoadingUI::UpdateLoadingText,
			0.5f,
			true
		);
	}
}

void ULoadingUI::NativeDestruct()
{
	Super::NativeDestruct();

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LoadingDotTimerHandle);
	}
}


void ULoadingUI::connect(int num) {

	for (int i = 0; i < num; ++i) {

		Players[i]->SetColorAndOpacity(FLinearColor(0.74f, 0.74f, 0.74f, 1.f));
	}
}


void ULoadingUI:: logout() {
	for (int i = 0; i < OnlineP; ++i) {

		Players[2-i]->SetColorAndOpacity(FLinearColor(0.0625f, 0.0625f, 0.0625f, 1.f));
	}
}

void ULoadingUI::UpdateLoadingText()
{
	if (!IsValid(loadingmemo))
	{
		return;
	}

	DotCount++;

	if (DotCount > 3)
	{
		DotCount = 0;
	}

	FString DotString;

	for (int32 i = 0; i < DotCount; ++i)
	{
		DotString += TEXT(".");
	}
	FString BaseText = TEXT("Loading");

	if (setText.IsValidIndex(CurrentTextIndex))
	{
		BaseText = setText[CurrentTextIndex].ToString();
	}

	FString LoadingString = FString::Printf(TEXT("%s%s"), *BaseText, *DotString);

	loadingmemo->SetText(FText::FromString(LoadingString));
}