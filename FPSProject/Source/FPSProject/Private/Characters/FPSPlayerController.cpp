// Fill out your copyright notice in the Description page of Project Settings.



#include "Characters/FPSPlayerController.h"
#include "HUD/InventoryWidget.h"
#include "HUD/BaseUI.h"
#include "HUD/BasicUI.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"


void AFPSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!InvenWidgetClass||!TimerWidgetClass) return;

	InventoryW = CreateWidget<UInventoryWidget>(this, InvenWidgetClass);
	TimerW = CreateWidget<UBaseUI>(this, TimerWidgetClass);
	BasicW = CreateWidget<UBasicUI>(this, BasicWidgetClass);
	//여기서 위젯 생성 후 플레이어에서 뷰포트에 추가함. 여기서 뷰포트 추가하면 순서때문에 화면에 안그려짐.

	//입력 시스템 등록
	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());

	if (Subsystem) {
		Subsystem->AddMappingContext(PlayerMappingContext, 0);
	}
}

void AFPSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput =
		Cast<UEnhancedInputComponent>(InputComponent);

	if (EnhancedInput)
	{
		EnhancedInput->BindAction(IA_SelectSlot1, ETriggerEvent::Started, this, &AFPSPlayerController::Pressed1);
		EnhancedInput->BindAction(IA_SelectSlot2, ETriggerEvent::Started, this, &AFPSPlayerController::Pressed2);
		EnhancedInput->BindAction(IA_SelectSlot3, ETriggerEvent::Started, this, &AFPSPlayerController::Pressed3);
		EnhancedInput->BindAction(IA_SelectSlot4, ETriggerEvent::Started, this, &AFPSPlayerController::Pressed4);
		EnhancedInput->BindAction(IA_SelectSlot5, ETriggerEvent::Started, this, &AFPSPlayerController::Pressed5);
	}
}

void AFPSPlayerController::Pressed1(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Warning, TEXT("1 Key Pressed"));

	// 여기서 인벤토리 / 무기 변경 / 슬롯 선택 처리
	InventoryW->SelectSlot(0); // 예시로 슬롯 1 선택
}

void AFPSPlayerController::Pressed2(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Warning, TEXT("2 Key Pressed"));

	// 여기서 인벤토리 / 무기 변경 / 슬롯 선택 처리
	InventoryW->SelectSlot(1); // 예시로 슬롯 1 선택
}

void AFPSPlayerController::Pressed3(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Warning, TEXT("1 Key Pressed"));

	// 여기서 인벤토리 / 무기 변경 / 슬롯 선택 처리
	InventoryW->SelectSlot(2); // 예시로 슬롯 1 선택
}

void AFPSPlayerController::Pressed4(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Warning, TEXT("1 Key Pressed"));

	// 여기서 인벤토리 / 무기 변경 / 슬롯 선택 처리
	InventoryW->SelectSlot(3); // 예시로 슬롯 1 선택
}

void AFPSPlayerController::Pressed5(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Warning, TEXT("1 Key Pressed"));

	// 여기서 인벤토리 / 무기 변경 / 슬롯 선택 처리
	InventoryW->SelectSlot(4); // 예시로 슬롯 1 선택
}

bool AFPSPlayerController::PickUp_Item(UTexture2D* image, int32 handw)
{
	bool t=false;
	if (InventoryW)
	{
		t=InventoryW->PickUp_Item(image, handw);
	}
	return t;
}
