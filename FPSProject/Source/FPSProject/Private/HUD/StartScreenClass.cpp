// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/StartScreenClass.h"

void UStartScreenClass::NativeConstruct()
{
	Super::NativeConstruct();

}

void UStartScreenClass::PlayAni_Start()
{
	PlayAnimation(StartPop);
}
