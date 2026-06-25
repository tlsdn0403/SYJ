#include "Characters/FPSBaseCharacter.h"

#include "Characters/FPSPlayerController.h"
#include "Components/HealthComponent.h"
#include "FPSProjectGameInstance.h"
#include "HUD/BasicUI.h"
#include "HUD/L2BaseUI.h"
#include "Kismet/GameplayStatics.h"
#include "Truck/Truck.h"

AFPSPlayerController* AFPSBaseCharacter::ResolveHealthHUDController() const
{
	if (AFPSPlayerController* PlayerController = Cast<AFPSPlayerController>(GetController()))
	{
		return PlayerController;
	}

	AFPSPlayerController* LocalPlayerController = Cast<AFPSPlayerController>(
		UGameplayStatics::GetPlayerController(this, 0));
	if (!LocalPlayerController)
	{
		return nullptr;
	}

	if (const UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(GetGameInstance()))
	{
		if (GameInstance->MyPlayer == this)
		{
			return LocalPlayerController;
		}
	}

	const ATruck* RelevantTruck = DisplayedHealthTruck ? DisplayedHealthTruck.Get() : CurrentTruck;
	if (RelevantTruck && RelevantTruck->GetController() == LocalPlayerController)
	{
		return LocalPlayerController;
	}

	return nullptr;
}

void AFPSBaseCharacter::UpdateHealthHUD(float CurrentHealth, float MaxHealth)
{
	if (AFPSPlayerController* PlayerController = ResolveHealthHUDController())
	{
		if (PlayerController->BasicW)
		{
			PlayerController->BasicW->SetHealth(CurrentHealth, MaxHealth);
		}
	}
}

void AFPSBaseCharacter::ShowTruckHealthOnHUD(ATruck* Truck)
{
	if (!Truck || !ResolveHealthHUDController())
	{
		return;
	}

	if (DisplayedHealthTruck && DisplayedHealthTruck != Truck)
	{
		DisplayedHealthTruck->OnTruckHealthChanged.RemoveDynamic(
			this,
			&AFPSBaseCharacter::HandleDisplayedTruckHealthChanged);
	}

	DisplayedHealthTruck = Truck;
	Truck->OnTruckHealthChanged.AddUniqueDynamic(
		this,
		&AFPSBaseCharacter::HandleDisplayedTruckHealthChanged);
	UpdateHealthHUD(Truck->GetTruckHealth(), Truck->GetTruckMaxHealth());
}

void AFPSBaseCharacter::StopShowingTruckHealthOnHUD()
{
	if (DisplayedHealthTruck)
	{
		DisplayedHealthTruck->OnTruckHealthChanged.RemoveDynamic(
			this,
			&AFPSBaseCharacter::HandleDisplayedTruckHealthChanged);
		DisplayedHealthTruck = nullptr;
	}

	if (HealthComponent)
	{
		UpdateHealthHUD(HealthComponent->GetHealth(), HealthComponent->MaxGetHealth());
	}
}

void AFPSBaseCharacter::RestorePlayerHealthOnHUD()
{
	StopShowingTruckHealthOnHUD();
}

void AFPSBaseCharacter::HandleDisplayedTruckHealthChanged(float CurrentHealth, float MaxHealth)
{
	UpdateHealthHUD(CurrentHealth, MaxHealth);
}

void AFPSBaseCharacter::UpdateFuelHUD(float CurrentFuel, float MaxFuel)
{
	if (AFPSPlayerController* PlayerController = ResolveHealthHUDController())
	{
		if (PlayerController->L2BaseW)
		{
			PlayerController->L2BaseW->OilUpdate(CurrentFuel, MaxFuel);
		}
	}
}

void AFPSBaseCharacter::ShowTruckFuelOnHUD(ATruck* Truck)
{
	if (!Truck || !ResolveHealthHUDController())
	{
		return;
	}

	if (DisplayedFuelTruck && DisplayedFuelTruck != Truck)
	{
		DisplayedFuelTruck->OnTruckFuelChanged.RemoveDynamic(
			this,
			&AFPSBaseCharacter::HandleDisplayedTruckFuelChanged);
	}

	DisplayedFuelTruck = Truck;
	Truck->OnTruckFuelChanged.AddUniqueDynamic(
		this,
		&AFPSBaseCharacter::HandleDisplayedTruckFuelChanged);
	UpdateFuelHUD(Truck->GetTruckFuel(), Truck->GetTruckMaxFuel());
}

void AFPSBaseCharacter::StopShowingTruckFuelOnHUD()
{
	if (DisplayedFuelTruck)
	{
		DisplayedFuelTruck->OnTruckFuelChanged.RemoveDynamic(
			this,
			&AFPSBaseCharacter::HandleDisplayedTruckFuelChanged);
		DisplayedFuelTruck = nullptr;
	}
}

void AFPSBaseCharacter::HandleDisplayedTruckFuelChanged(float CurrentFuel, float MaxFuel)
{
	UpdateFuelHUD(CurrentFuel, MaxFuel);
}
