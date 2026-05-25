// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EffectUI.generated.h"

/**
 * 
 */
class UImage;
class UBloodEfWidget;
class UCanvasPanel;

UCLASS()
class FPSPROJECT_API UEffectUI : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* BaseCanvas;

	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* B_EdgeAni; //체력 몇 이하면 이 애니메이션 계속 유지.


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UBloodEfWidget> BloodWidgetClass;

	UPROPERTY()
	UBloodEfWidget* BloodW;

	//UPROPERTY(meta = (BindWidgetAnim), Transient)
	//UWidgetAnimation* B_EffectAni; //좀비들 들이박을때 팍 하고 튀겎뜸...

public:
	bool replay = false;

	UPROPERTY(meta = (BindWidget))	//
		UImage* BloodEdge;

	//UPROPERTY(meta = (BindWidget))	//
	//	UImage* BloodEf;

	//void PlayAni_Edge() { PlayAnimation(B_EdgeAni); }
	void PlayAni_Effect(bool re);
	void SpawnBloodEffects();


};
