// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Zombie/BaseZombie.h"
#include "BP_Zombiegirl.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API ABP_Zombiegirl : public ABaseZombie
{
	GENERATED_BODY()
	
protected:
	virtual void InitializeBoneDurability() override;
	virtual FName GetParentBoneForDamage(FName HitBoneName) const override;
	virtual FName GetPhysicsRootBoneName() const override;
	virtual bool IsFatalDismemberBone(FName BoneName) const override;
	virtual bool IsLegBone(FName BoneName) const override;
};
