// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/HUD_Base.h"
#include "HUD/InventoryWidget.h"


void AHUD_Base::BeginPlay()
{
	Super::BeginPlay();

	/*InventoryWidget = CreateWidget<UInventoryWidget>(GetWorld()->Getplayerpawn, InventoryClass);
	InventoryWidget->AddToViewport();*/

}
