// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/InteractUIClass.h"

void UInteractUIClass::PlayAni_PopUp()
{
	if (PopUp) PlayAnimation(PopUp);
}