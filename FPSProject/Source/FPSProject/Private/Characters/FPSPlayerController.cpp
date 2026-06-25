// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/FPSPlayerController.h"
#include "Characters/FPSBaseCharacter.h"
#include "HUD/InventoryWidget.h"
#include "HUD/BaseUI.h"
#include "HUD/BasicUI.h"
#include "HUD/EffectUI.h"
#include "HUD/L2BaseUI.h"
#include "FPSProjectGameInstance.h"
#include<algorithm>
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"


void AFPSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!InvenWidgetClass || !TimerWidgetClass) return;

	InventoryW = CreateWidget<UInventoryWidget>(this, InvenWidgetClass);
	TimerW = CreateWidget<UBaseUI>(this, TimerWidgetClass);
	BasicW = CreateWidget<UBasicUI>(this, BasicWidgetClass);
	EffectW = CreateWidget<UEffectUI>(this, EffectWidgetClass);
	L2BaseW = CreateWidget<UL2BaseUI>(this, L2BaseWidgetClass);

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
		EnhancedInput->BindAction(IA_TAB, ETriggerEvent::Started, this, &AFPSPlayerController::PressedTAB);
	}
}

void AFPSPlayerController::Pressed1(const FInputActionValue& Value)
{

	if (TrySpectatePlayerSlot(0))
	{
		return;
	}
	if (AFPSBaseCharacter* ControlledCharacter = Cast<AFPSBaseCharacter>(GetPawn()))
	{
		if (ControlledCharacter->IsDead())
		{
			return;
		}
	}

	// 여기서 인벤토리 / 무기 변경 / 슬롯 선택 처리
}

void AFPSPlayerController::Pressed2(const FInputActionValue& Value)
{

	if (TrySpectatePlayerSlot(1))
	{
		return;
	}
	if (AFPSBaseCharacter* ControlledCharacter = Cast<AFPSBaseCharacter>(GetPawn()))
	{
		if (ControlledCharacter->IsDead())
		{
			return;
		}
	}

	bool success = L2BaseW->UsingItem(2);
	if (success) {
		AFPSBaseCharacter* player = Cast<AFPSBaseCharacter>(GetPawn());
		if (player)
		{
			//여기에 100이면 사용못하게 코드 추가해여ㅑ함!!!
			player->Heal(20.f);
		}
	}

}

void AFPSPlayerController::Pressed3(const FInputActionValue& Value)
{

	InventoryW->SelectSlot(2);
}

void AFPSPlayerController::Pressed4(const FInputActionValue& Value)
{

	InventoryW->SelectSlot(3);
}

void AFPSPlayerController::Pressed5(const FInputActionValue& Value)
{

	InventoryW->SelectSlot(4);
}

void AFPSPlayerController::PressedTAB(const FInputActionValue& Value)
{
	openItem = !openItem;
	if (openItem) {
		L2BaseW->PlayAnimation(L2BaseW->Ani_ItemOpen);

	}
	else {
		L2BaseW->PlayAnimationReverse(L2BaseW->Ani_ItemOpen);
	}
}

bool AFPSPlayerController::TrySpectatePlayerSlot(int32 SlotIndex)
{
	AFPSBaseCharacter* ControlledCharacter = Cast<AFPSBaseCharacter>(GetPawn());
	if (ControlledCharacter == nullptr || !ControlledCharacter->IsDead())
	{
		return false;
	}

	UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance());
	if (GameInstance == nullptr)
	{
		return false;
	}

	if (AFPSBaseCharacter* SpectateTarget = GameInstance->GetSpectateTargetBySlot(SlotIndex))
	{
		SetViewTargetWithBlend(SpectateTarget, 0.2f);
		return true;
	}

	return false;
}

bool AFPSPlayerController::PickUp_Item(UTexture2D* image, int32 handw)
{
	bool t = false;
	if (InventoryW)
	{
		t = InventoryW->PickUp_Item(image, handw);
	}
	return t;
}