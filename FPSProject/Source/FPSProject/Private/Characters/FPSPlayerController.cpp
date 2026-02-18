// Fill out your copyright notice in the Description page of Project Settings.



#include "Characters/FPSPlayerController.h"
#include "HUD/InventoryWidget.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"


void AFPSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!InvenWidgetClass) return;

	InventoryW = CreateWidget<UInventoryWidget>(this, InvenWidgetClass);
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
	}
}

void AFPSPlayerController::Pressed1(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Warning, TEXT("1 Key Pressed"));

	// 여기서 인벤토리 / 무기 변경 / 슬롯 선택 처리
}