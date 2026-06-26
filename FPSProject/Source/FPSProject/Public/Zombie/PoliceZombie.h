// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Zombie/MixamoZombie.h"
#include "PoliceZombie.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, meta = (DisplayName = "Mixamo Police Zombie"))
class FPSPROJECT_API APoliceZombie : public AMixamoZombie
{
	GENERATED_BODY()

public:
	APoliceZombie();

protected:
	virtual void BeginPlay() override;
	virtual FVector GetCrawlingMeshRelativeLocation(const FVector& CurrentStandingMeshRelativeLocation) const override;
	virtual void InitializeBoneDurability() override;
	virtual FName GetParentBoneForDamage(FName HitBoneName) const override;
	virtual FName GetPhysicsRootBoneName() const override;
	virtual bool IsFatalDismemberBone(FName BoneName) const override;
	virtual bool IsLegBone(FName BoneName) const override;
};