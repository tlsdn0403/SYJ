// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartScreenClass.generated.h"

/**
 * 
 */


class UEditableText;
class UButton;
class UImage;
class UWidgetAnimation;

UCLASS()
class FPSPROJECT_API UStartScreenClass : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	//virtual bool Initialize() override;		//마우스 입력 받는 용


	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* StartPop;

	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* Click;

	UPROPERTY(meta = (BindWidget))
	UImage* LogoImage;

	UPROPERTY(meta = (BindWidget))
	UEditableText* NicknameT;

	UPROPERTY(meta = (BindWidget))
	UEditableText* IPT;

public:

	UPROPERTY(meta = (BindWidget))
	class UButton* LoginButton;
	UFUNCTION()
	void PlayAni_Start();
	UFUNCTION()
	void PlayAni_Click();
	UFUNCTION()
	void OnClickLogin();
};


