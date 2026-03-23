// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/StartModeController.h"
#include "HUD/StartScreenClass.h"

void AStartModeController::BeginPlay() {
		Super::BeginPlay();
	if ( !StartScreenWidgetClass) return;

	bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	StartSW = CreateWidget<UStartScreenClass>(this, StartScreenWidgetClass);
	StartGame();
}

void AStartModeController::StartGame()
{
	// UI 积己, 局聪皋捞记 殿
	StartSW->AddToViewport();
	StartSW->PlayAni_Start();
}