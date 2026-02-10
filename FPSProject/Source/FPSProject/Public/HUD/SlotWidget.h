// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SlotWidget.generated.h"

/**
 * 
 */

class UImage;
class UTexture2D;

UCLASS()
class FPSPROJECT_API USlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;

protected:
	// 슬롯 이미지 (손이나 아이템 아이콘 들어감)
	/*UPROPERTY(meta = (BindWidget), Category = "MySetting")
	UImage* ItemIcon;*/

	void SetItem(UTexture2D* IconTexture);
	void ClearSlot();	//

};
