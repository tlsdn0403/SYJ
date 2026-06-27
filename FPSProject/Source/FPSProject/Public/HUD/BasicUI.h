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
class UButton;
class UWidgetAnimation;

UCLASS()
class FPSPROJECT_API UBasicUI : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	//
	UPROPERTY(meta = (BindWidgetAnim, AllowPrivateAccess), Transient)
	UWidgetAnimation* Ani_ESC;

	UPROPERTY(meta = (BindWidget))
	UButton* EndB;		//나가기 버튼

	UPROPERTY(meta = (BindWidget))
	UButton* ReB;		//계속하기 버튼

public:


	UPROPERTY(meta = (BindWidget))
	UTextBlock* NameText;

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
	void SetAmmoCount(int32 AmmoCount);

	UFUNCTION()
	void OnResumeClicked();
	UFUNCTION()
	void OnExitClicked();

	void Play_ESC();
};
