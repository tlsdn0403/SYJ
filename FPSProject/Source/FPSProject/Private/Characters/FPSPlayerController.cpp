// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/FPSPlayerController.h"
#include "Characters/FPSBaseCharacter.h"
#include "HUD/InventoryWidget.h"
#include "HUD/BaseUI.h"
#include "HUD/BasicUI.h"
#include "HUD/EffectUI.h"
#include "HUD/L2BaseUI.h"
#include "HUD/MachineGunUI.h"
#include "FPSStage2WorldUtils.h"
#include "FPSProjectGameInstance.h"
#include<algorithm>
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
AFPSBaseCharacter* ResolveLocalStageCharacter(AFPSPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return nullptr;
	}

	if (AFPSBaseCharacter* ControlledCharacter = Cast<AFPSBaseCharacter>(PlayerController->GetPawn()))
	{
		return ControlledCharacter;
	}

	if (const UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(PlayerController->GetGameInstance()))
	{
		if (IsValid(GameInstance->MyPlayer))
		{
			return GameInstance->MyPlayer;
		}
	}

	return nullptr;
}
}

AFPSPlayerController::AFPSPlayerController()
{
	static ConstructorHelpers::FClassFinder<UMachineGunUI> MachineGunWidgetBP(TEXT("/Game/HUD/WBP_machine_Gun"));
	if (MachineGunWidgetBP.Succeeded())
	{
		MachineGunWidgetClass = MachineGunWidgetBP.Class;
	}
}

void AFPSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (MachineGunWidgetClass)
	{
		MachineGunW = CreateWidget<UMachineGunUI>(this, MachineGunWidgetClass);
		if (MachineGunW)
		{
			MachineGunW->SetVisibleState(false);
		}
	}

	if (!InvenWidgetClass || !TimerWidgetClass) return;

	InventoryW = CreateWidget<UInventoryWidget>(this, InvenWidgetClass);
	TimerW = CreateWidget<UBaseUI>(this, TimerWidgetClass);
	BasicW = CreateWidget<UBasicUI>(this, BasicWidgetClass);
	EffectW = CreateWidget<UEffectUI>(this, EffectWidgetClass);
	L2BaseW = CreateWidget<UL2BaseUI>(this, L2BaseWidgetClass);

	if (AFPSBaseCharacter* ControlledCharacter = Cast<AFPSBaseCharacter>(GetPawn()))
	{
		if (FPSStage2WorldUtils::IsStage2World(GetWorld()))
		{
			ControlledCharacter->Delete_L1Widget(this);
			ControlledCharacter->Add_L2_Widget(this);
		}
		else
		{
			//ControlledCharacter->Add_L1_Widget(this);
			ControlledCharacter->Add_L2_Widget(this);
		}
	}

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
	if (AFPSBaseCharacter* ControlledCharacter = ResolveLocalStageCharacter(this))
	{
		if (ControlledCharacter->IsDead())
		{
			return;
		}

		if (const UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(ControlledCharacter->GetGameInstance()))
		{
			if (GameInstance->IsInStage2World())
			{
				ControlledCharacter->UseFuelCan();
				return;
			}
		}
	}
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
			player->Heal(20.f);
		}
	}

}

void AFPSPlayerController::Pressed3(const FInputActionValue& Value)
{

	if (TrySpectatePlayerSlot(2))
	{
		return;
	}
	if (AFPSBaseCharacter* ControlledCharacter = ResolveLocalStageCharacter(this))
	{
		if (ControlledCharacter->IsDead())
		{
			return;
		}

		if (const UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(ControlledCharacter->GetGameInstance()))
		{
			if (GameInstance->IsInStage2World())
			{
				ControlledCharacter->UseTruckRepairKit();
				return;
			}
		}
	}

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
