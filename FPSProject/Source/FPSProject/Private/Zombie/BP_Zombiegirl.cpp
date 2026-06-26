// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/BP_Zombiegirl.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"

ABP_Zombiegirl::ABP_Zombiegirl()
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> GirlMesh(
		TEXT("/Script/Engine.SkeletalMesh'/Game/Zombie/mixamo/ch/zom_girl/Zombiegirl_W_Kurniawan.Zombiegirl_W_Kurniawan'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> GirlIdle(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/ayjstart_Anim.ayjstart_Anim'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> GirlWalk(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/Zombie_Walk.Zombie_Walk'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> GirlAttackOne(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/attack.attack'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> GirlAttackTwo(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/Attack2.Attack2'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> GirlTruckAttackOne(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/TruckAttack.TruckAttack'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> GirlTruckAttackTwo(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/truckattack2.truckattack2'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> GirlDeathOne(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/Zombie_Dying__1_.Zombie_Dying__1_'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> GirlDeathTwo(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/HeadDying.HeadDying'"));

	if (GirlMesh.Succeeded() && GetMesh())
	{
		GetMesh()->SetSkeletalMesh(GirlMesh.Object);
		GetMesh()->SetRelativeLocation(FVector(-2.0f, 0.0f, -65.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	IdleAnimation = GirlIdle.Object;
	WalkAnimation = GirlWalk.Object;
	AttackAnimations.Reset();
	if (GirlAttackOne.Succeeded()) AttackAnimations.Add(GirlAttackOne.Object);
	if (GirlAttackTwo.Succeeded()) AttackAnimations.Add(GirlAttackTwo.Object);
	TruckAttackAnimations.Reset();
	if (GirlTruckAttackOne.Succeeded()) TruckAttackAnimations.Add(GirlTruckAttackOne.Object);
	if (GirlTruckAttackTwo.Succeeded()) TruckAttackAnimations.Add(GirlTruckAttackTwo.Object);
	DeathAnimation = GirlDeathOne.Object;
	DeathAnimations.Reset();
	if (GirlDeathOne.Succeeded()) DeathAnimations.Add(GirlDeathOne.Object);
	if (GirlDeathTwo.Succeeded()) DeathAnimations.Add(GirlDeathTwo.Object);
}

FVector ABP_Zombiegirl::GetCrawlingMeshRelativeLocation(const FVector& CurrentStandingMeshRelativeLocation) const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float GroundedMeshZ = Capsule
		? -Capsule->GetUnscaledCapsuleHalfHeight()
		: CurrentStandingMeshRelativeLocation.Z;

	return FVector(
		CurrentStandingMeshRelativeLocation.X,
		CurrentStandingMeshRelativeLocation.Y,
		FMath::Max(CurrentStandingMeshRelativeLocation.Z, GroundedMeshZ));
}

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
	const FString LowerBoneString = BoneString.ToLower();

	UE_LOG(LogTemp, Verbose, TEXT("ZombieGirl Hit Bone: %s"), *BoneString);

	if (LowerBoneString == TEXT("head"))
	{
		return FName("Head");
	}
	if (LowerBoneString == TEXT("upperarm_l"))
	{
		return FName("LeftArm");
	}
	if (LowerBoneString == TEXT("lowerarm_l"))
	{
		return FName("LeftForeArm");
	}
	if (LowerBoneString == TEXT("upperarm_r"))
	{
		return FName("RightArm");
	}
	if (LowerBoneString == TEXT("lowerarm_r"))
	{
		return FName("RightForeArm");
	}
	if (LowerBoneString == TEXT("thigh_l"))
	{
		return FName("LeftUpLeg");
	}
	if (LowerBoneString == TEXT("calf_l"))
	{
		return FName("LeftLeg");
	}
	if (LowerBoneString == TEXT("thigh_r"))
	{
		return FName("RightUpLeg");
	}
	if (LowerBoneString == TEXT("calf_r"))
	{
		return FName("RightLeg");
	}
	if (LowerBoneString == TEXT("spine_01"))
	{
		return FName("Spine");
	}

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