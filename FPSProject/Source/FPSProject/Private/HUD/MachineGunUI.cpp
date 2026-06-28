#include "HUD/MachineGunUI.h"

#include "Components/RadialSlider.h"
#include "Components/TextBlock.h"

void UMachineGunUI::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshText();
	RefreshAmmoSlider();
}

void UMachineGunUI::SetMachineGunAmmo(int32 TotalGunAmmo, int32 CurrentGunAmmo, int32 MaxGunAmmo)
{
	CachedTotalGunAmmo = FMath::Max(TotalGunAmmo, 0);
	CachedMaxGunAmmo = FMath::Max(MaxGunAmmo, 0);
	CachedCurrentGunAmmo = FMath::Clamp(CurrentGunAmmo, 0, CachedMaxGunAmmo);
	RefreshText();
	RefreshAmmoSlider();
}

void UMachineGunUI::SetVisibleState(bool bShouldShow)
{
	SetVisibility(bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UMachineGunUI::RefreshText()
{
	if (TotalGun)
	{
		TotalGun->SetText(FText::AsNumber(CachedTotalGunAmmo));
	}

	if (MaxGun)
	{
		MaxGun->SetText(FText::AsNumber(CachedMaxGunAmmo));
	}

	if (CurrentGun)
	{
		CurrentGun->SetText(FText::AsNumber(CachedCurrentGunAmmo));
	}
}

void UMachineGunUI::RefreshAmmoSlider()
{
	if (!RadialSlider)
	{
		return;
	}

	const float AmmoRatio = CachedMaxGunAmmo > 0
		? FMath::Clamp(static_cast<float>(CachedCurrentGunAmmo) / static_cast<float>(CachedMaxGunAmmo), 0.0f, 1.0f)
		: 0.0f;

	RadialSlider->SetValue(AmmoRatio);
}
