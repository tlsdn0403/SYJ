// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/EffectUI.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UEffectUI::NativeConstruct()
{
	Super::NativeConstruct();
}

void UEffectUI::PlayAni_Effect(bool re)
{
	replay = re;
	if (replay) {
		PlayAnimation(B_EdgeAni, 0.0f, 0);
	}
	else {
		StopAnimation(B_EdgeAni);
	}
}