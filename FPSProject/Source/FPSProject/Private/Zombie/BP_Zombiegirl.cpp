// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/BP_Zombiegirl.h"

void ABP_Zombiegirl::InitializeBoneDurability()
{
	ResetDismemberBones();

	RegisterDismemberBone(FName("Head"), 10.0f);

	RegisterDismemberBone(FName("LeftArm"), 15.0f);
	RegisterDismemberBone(FName("LeftForeArm"), 10.0f);
	RegisterDismemberBone(FName("RightArm"), 15.0f);
	RegisterDismemberBone(FName("RightForeArm"), 10.0f);

	RegisterDismemberBone(FName("LeftUpLeg"), 20.0f);
	RegisterDismemberBone(FName("LeftLeg"), 15.0f);
	RegisterDismemberBone(FName("RightUpLeg"), 20.0f);
	RegisterDismemberBone(FName("RightLeg"), 15.0f);

	RegisterDismemberBone(FName("Spine"), 50.0f);
}

FName ABP_Zombiegirl::GetParentBoneForDamage(FName HitBoneName) const
{
	const FString BoneString = HitBoneName.ToString();

	UE_LOG(LogTemp, Log, TEXT("ZombieGirl Hit Bone: %s"), *BoneString);

	if (BoneString.Contains("Head") ||
		BoneString.Contains("HeadTop") ||
		BoneString.Contains("Neck") ||
		BoneString.Contains("Eye"))
	{
		return FName("Head");
	}

	if (BoneString.Contains("Left"))
	{
		if (BoneString.Contains("UpLeg"))
		{
			return FName("LeftUpLeg");
		}
		if (BoneString.Contains("Leg") ||
			BoneString.Contains("Foot") ||
			BoneString.Contains("Toe"))
		{
			return FName("LeftLeg");
		}

		if (BoneString.Contains("ForeArm") ||
			BoneString.Contains("Hand") ||
			BoneString.Contains("Index") ||
			BoneString.Contains("Middle") ||
			BoneString.Contains("Pinky") ||
			BoneString.Contains("Ring") ||
			BoneString.Contains("Thumb"))
		{
			return FName("LeftForeArm");
		}
		if (BoneString.Contains("Shoulder") || BoneString.Contains("Arm"))
		{
			return FName("LeftArm");
		}
	}

	if (BoneString.Contains("Right"))
	{
		if (BoneString.Contains("UpLeg"))
		{
			return FName("RightUpLeg");
		}
		if (BoneString.Contains("Leg") ||
			BoneString.Contains("Foot") ||
			BoneString.Contains("Toe"))
		{
			return FName("RightLeg");
		}

		if (BoneString.Contains("ForeArm") ||
			BoneString.Contains("Hand") ||
			BoneString.Contains("Index") ||
			BoneString.Contains("Middle") ||
			BoneString.Contains("Pinky") ||
			BoneString.Contains("Ring") ||
			BoneString.Contains("Thumb"))
		{
			return FName("RightForeArm");
		}
		if (BoneString.Contains("Shoulder") || BoneString.Contains("Arm"))
		{
			return FName("RightArm");
		}
	}

	if (BoneString.Contains("Spine") || BoneString.Contains("Hips"))
	{
		return FName("Spine");
	}

	return HitBoneName;
}

FName ABP_Zombiegirl::GetPhysicsRootBoneName() const
{
	return FName("Hips");
}

bool ABP_Zombiegirl::IsFatalDismemberBone(FName BoneName) const
{
	return BoneName == FName("Head") || BoneName == FName("Spine");
}

bool ABP_Zombiegirl::IsLegBone(FName BoneName) const
{
	static const TArray<FName> LegBones = {
		FName("LeftUpLeg"), FName("LeftLeg"),
		FName("RightUpLeg"), FName("RightLeg")
	};

	return LegBones.Contains(BoneName);
}
