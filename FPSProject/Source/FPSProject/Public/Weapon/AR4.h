// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "AR4.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API AAR4 : public AWeaponBase
{
	GENERATED_BODY()
	

public:
	

protected:
	virtual void AttachWeapon(AFPSBaseCharacter* TargetCharacter) override;

	virtual void Fire() override;
};
