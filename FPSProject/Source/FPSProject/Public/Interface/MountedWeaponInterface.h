#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MountedWeaponInterface.generated.h"

class AFPSBaseCharacter;

UINTERFACE(BlueprintType)
class FPSPROJECT_API UMountedWeaponInterface : public UInterface
{
	GENERATED_BODY()
};

class FPSPROJECT_API IMountedWeaponInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mounted Weapon")
	void BeginMountedUse(AFPSBaseCharacter* Character);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mounted Weapon")
	void EndMountedUse(AFPSBaseCharacter* Character);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mounted Weapon")
	void FireMountedWeapon(AFPSBaseCharacter* Character);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mounted Weapon")
	void UpdateMountedAim(AFPSBaseCharacter* Character, const FRotator& ControlRotation);
};
