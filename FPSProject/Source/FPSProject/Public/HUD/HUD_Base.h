// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HUD_Base.generated.h"

/**
 * 
 */

class UInventoryWidget;
UCLASS()
class FPSPROJECT_API AHUD_Base : public AHUD
{
	GENERATED_BODY()
	
public:
    //위젯
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
    TSubclassOf<UInventoryWidget> InventoryClass; //어떤 위젯 사용할것인지에 대한 변수

    UPROPERTY()
    UInventoryWidget* InventoryWidget;
};
