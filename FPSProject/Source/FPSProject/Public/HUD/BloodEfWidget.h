// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BloodEfWidget.generated.h"

/**
 *
 */
class UImage;
class UWidgetAnimation;

UCLASS()
class FPSPROJECT_API UBloodEfWidget : public UUserWidget
{
	GENERATED_BODY()
protected:

	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* B_EffectAni; //¡ª∫ÒµÈ µÈ¿Ãπ⁄¿ª∂ß ∆≈ «œ∞Ì ∆¢Åß∂‰...

public:
	bool replay = false;

	UPROPERTY(meta = (BindWidget))	//
	UImage* BloodEf;

	void PlayAni_Ef()
	{
		if (B_EffectAni)
		{
			PlayAnimation(B_EffectAni);
		}
	}

	UFUNCTION()
	void OnEffectAniFinished();

};
