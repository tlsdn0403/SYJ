// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "L2BaseUI.generated.h"

/**
 * 
 */

class UImage;
class UTextBlock;

UCLASS()
class FPSPROJECT_API UL2BaseUI : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

public:

	UPROPERTY(meta = (BindWidget))	//
		UImage* HealPack;

	//UPROPERTY(meta = (BindWidget))
	//UTextBlock* GunText;

	//UPROPERTY(meta = (BindWidget))	//
	//	UImage* HealthP;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* HealPackText;



	//void GetGunAR4();
	//void SetHealth(float CurrentHP, float MaxHP);

	
};
