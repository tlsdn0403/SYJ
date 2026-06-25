// Fill out your copyright notice in the Description page of Project Settings.

#include "HUD/BaseUI.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Truck/Truck.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"

void UBaseUI::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateTimerText();
}

void UBaseUI::SetRemainingTime(int32 NewRemainingTime)
{
	TotalTime = FMath::Max(NewRemainingTime, 0);
	UpdateTimerText();

	if (TotalTime <= 0)
	{
		HandleTimerFinished();
	}
}

void UBaseUI::UpdateTimerText()
{
	if (TimerText == nullptr)
	{
		return;
	}

	if (TotalTime != LastDisplayedTime)
	{
		if (TotalTime > 0 && TotalTime <= 5)
		{
			PlayAni_PopUp();
		}
		else if (TotalTime > 5 && TotalTime <= 20)
		{
			if (ClockS && TotalTime > 17 && TotalTime <= 20)
			{
				UGameplayStatics::PlaySound2D(this, ClockS);
			}
			PlayAni_Vibration();
		}
	}

	const FString TimeText = FString::Printf(TEXT("%02d:%02d"), TotalTime / 60, TotalTime % 60);
	TimerText->SetText(FText::FromString(TimeText));
	LastDisplayedTime = TotalTime;
}

void UBaseUI::HandleTimerFinished()
{
	if (bHasFinishedLoadingPhase)
	{
		return;
	}

	bHasFinishedLoadingPhase = true;
	FinishTruckLoadingPhase();

	if (Boom)
	{
		UGameplayStatics::PlaySound2D(this, Boom);
	}

	StopAllAnimations();
	RemoveFromParent();
}

void UBaseUI::FinishTruckLoadingPhase()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ATruck> It(World); It; ++It)
	{
		if (ATruck* Truck = *It)
		{
			Truck->SetLoadingPhase(false);
		}
	}
}