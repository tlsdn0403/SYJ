// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BasicUI.generated.h"

/**
 * 
 */
class UImage;
class UTextBlock;
class UProgressBar;

UCLASS()
class FPSPROJECT_API UBasicUI : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;


public:

	UPROPERTY(meta = (BindWidget))	//
	UImage* GunImage;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* GunText;

	UPROPERTY(meta = (BindWidget))	//
	UImage* HealthP;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* HpText;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* HpBar;

	void GetGunAR4();
	void SetHealth(float CurrentHP, float MaxHP);
	
};
