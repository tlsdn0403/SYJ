// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/BloodEfWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UBloodEfWidget::NativeConstruct()
{
	Super::NativeConstruct();

	FWidgetAnimationDynamicEvent EndDelegate;
	EndDelegate.BindDynamic(this, &UBloodEfWidget::OnEffectAniFinished);
	BindToAnimationFinished(B_EffectAni, EndDelegate);
}


void UBloodEfWidget::OnEffectAniFinished()
{
	RemoveFromParent();
}