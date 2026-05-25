// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EffectUI.generated.h"

/**
 * 
 */
class UImage;
class UProgressBar;

UCLASS()
class FPSPROJECT_API UEffectUI : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* B_EdgeAni; //체력 몇 이하면 이 애니메이션 계속 유지.

	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* B_EffectAni; //좀비들 들이박을때 팍 하고 튀겎뜸...

public:
	bool replay = false;

	UPROPERTY(meta = (BindWidget))	//
		UImage* BloodEdge;

	UPROPERTY(meta = (BindWidget))	//
		UImage* BloodEf;

	void PlayAni_Edge() { PlayAnimation(B_EdgeAni); }
	void PlayAni_Effect(bool re);


};
