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
	// UI 생성, 애니메이션 등
	StartSW->AddToViewport();
	StartSW->PlayAni_Start();
}