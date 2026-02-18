// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/FPSPlayerController.h"
#include "HUD/InventoryWidget.h"


void AFPSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!InvenWidgetClass) return;

	InventoryW = CreateWidget<UInventoryWidget>(this, InvenWidgetClass);
}