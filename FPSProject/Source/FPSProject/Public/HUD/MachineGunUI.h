#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MachineGunUI.generated.h"

class UTextBlock;
class URadialSlider;

UCLASS()
class FPSPROJECT_API UMachineGunUI : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "MachineGunUI")
	void SetMachineGunAmmo(int32 TotalGunAmmo, int32 CurrentGunAmmo, int32 MaxGunAmmo = 100);

	UFUNCTION(BlueprintCallable, Category = "MachineGunUI")
	void SetVisibleState(bool bShouldShow);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	UTextBlock* TotalGun;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	UTextBlock* MaxGun;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	UTextBlock* CurrentGun;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	URadialSlider* RadialSlider;

private:
	int32 CachedTotalGunAmmo = 0;
	int32 CachedMaxGunAmmo = 100;
	int32 CachedCurrentGunAmmo = 0;

	void RefreshText();
	void RefreshAmmoSlider();
};
