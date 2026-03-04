// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractUIClass.generated.h"

/**
 * 
 */

class UWidgetAnimation;

UCLASS()
class FPSPROJECT_API UInteractUIClass : public UUserWidget
{
	GENERATED_BODY()
public:
   
    UPROPERTY(meta = (BindWidgetAnim), Transient)
    UWidgetAnimation* PopUp;

    UFUNCTION(BlueprintCallable)
    void PlayAni_PopUp();
	
};
