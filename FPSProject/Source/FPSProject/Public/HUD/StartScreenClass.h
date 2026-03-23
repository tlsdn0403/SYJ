// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartScreenClass.generated.h"

/**
 * 
 */


class UImage;
class UWidgetAnimation;

UCLASS()
class FPSPROJECT_API UStartScreenClass : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* StartPop;

public:
	// 슬롯 이미지 (손이나 아이템 아이콘 들어감)	meta = (BindWidget)
	UPROPERTY(meta = (BindWidget))
	UImage* LogoImage;

	void PlayAni_Start();
	
};

