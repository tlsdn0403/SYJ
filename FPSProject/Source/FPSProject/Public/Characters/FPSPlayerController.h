// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "FPSPlayerController.generated.h"

/**
 * 
 */

class UInventoryWidget;
class UBaseUI;
class UEffectUI;
class UBasicUI;
class UInputMappingContext;
class UInputAction;

UCLASS()
class FPSPROJECT_API AFPSPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
public:

	// 인벤토리 위젯
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UInventoryWidget> InvenWidgetClass;

	// 타이머 위젯
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UBaseUI> TimerWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UEffectUI> EffectWidgetClass;

	UPROPERTY()
	UBaseUI* TimerW;

	//기본 위젯
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UBasicUI> BasicWidgetClass;

	UPROPERTY()
	UBasicUI* BasicW;


public:
	UPROPERTY()
	UInventoryWidget* InventoryW;

	UPROPERTY()
	UEffectUI* EffectW;

	// 입력 액션
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputMappingContext* PlayerMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* IA_SelectSlot1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* IA_SelectSlot2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* IA_SelectSlot3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* IA_SelectSlot4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* IA_SelectSlot5;


	// 실제 실행 함수
	UFUNCTION()
	void Pressed1(const FInputActionValue& InputValue);

	UFUNCTION()
	void Pressed2(const FInputActionValue& InputValue);

	UFUNCTION()
	void Pressed3(const FInputActionValue& InputValue);

	UFUNCTION()
	void Pressed4(const FInputActionValue& InputValue);

	UFUNCTION()
	void Pressed5(const FInputActionValue& InputValue);

	UFUNCTION()
	bool PickUp_Item(UTexture2D* image,int32 handw);

};

