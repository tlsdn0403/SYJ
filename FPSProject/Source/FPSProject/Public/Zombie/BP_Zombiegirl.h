// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Zombie/MixamoZombie.h"
#include "BP_Zombiegirl.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, meta = (DisplayName = "Mixamo Girl Zombie"))
class FPSPROJECT_API ABP_Zombiegirl : public AMixamoZombie
{
	GENERATED_BODY()

public:
	ABP_Zombiegirl();

protected:
	virtual FVector GetCrawlingMeshRelativeLocation(const FVector& CurrentStandingMeshRelativeLocation) const override;
	virtual void InitializeBoneDurability() override;
	virtual FName GetParentBoneForDamage(FName HitBoneName) const override;
	virtual FName GetPhysicsRootBoneName() const override;
	virtual bool IsFatalDismemberBone(FName BoneName) const override;
	virtual bool IsLegBone(FName BoneName) const override;
};
