// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractUIClass.generated.h"

/**
 * 
 */

class UWidgetAnimation;
class UTextBlock;

UCLASS()
class FPSPROJECT_API UInteractUIClass : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeConstruct() override;

public:
   
    UPROPERTY(meta = (BindWidgetAnim), Transient)
    UWidgetAnimation* PopUp;

    UFUNCTION(BlueprintCallable)
    void PlayAni_PopUp(bool bOpen);
    UFUNCTION(BlueprintCallable)
    void RePlayAni_PopUp();

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
    UTextBlock* InteractText;

    UFUNCTION(BlueprintCallable)
    void SetInteractText(const FText& NewText);

    void SetDoorInteractText(const bool bOpen);
	
};
