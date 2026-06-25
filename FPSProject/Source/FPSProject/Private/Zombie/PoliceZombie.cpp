// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/PoliceZombie.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"

APoliceZombie::APoliceZombie()
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> PoliceMesh(
		TEXT("/Script/Engine.SkeletalMesh'/Game/Zombie/mixamo/ch/zoM_police/copzombie_l_actisdato.copzombie_l_actisdato'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PoliceIdle(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/ch/zoM_police/copzombie_l_actisdato_Anim_Take_001.copzombie_l_actisdato_Anim_Take_001'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PoliceWalk(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/police/Zombie_Walk.Zombie_Walk'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PoliceAttackOne(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/police/attack.attack'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PoliceAttackTwo(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/police/Attack2.Attack2'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PoliceTruckAttackOne(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/police/TruckAttack.TruckAttack'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PoliceTruckAttackTwo(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/police/Truckattack2.Truckattack2'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PoliceDeathOne(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/police/Zombie_Dying__1_.Zombie_Dying__1_'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PoliceDeathTwo(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/police/HeadDying.HeadDying'"));

	if (PoliceMesh.Succeeded() && GetMesh())
	{
		GetMesh()->SetSkeletalMesh(PoliceMesh.Object);
		GetMesh()->SetRelativeLocation(FVector(-2.0f, 0.0f, -65.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	IdleAnimation = PoliceIdle.Object;
	WalkAnimation = PoliceWalk.Object;
	AttackAnimations.Reset();
	if (PoliceAttackOne.Succeeded()) AttackAnimations.Add(PoliceAttackOne.Object);
	if (PoliceAttackTwo.Succeeded()) AttackAnimations.Add(PoliceAttackTwo.Object);
	TruckAttackAnimations.Reset();
	if (PoliceTruckAttackOne.Succeeded()) TruckAttackAnimations.Add(PoliceTruckAttackOne.Object);
	if (PoliceTruckAttackTwo.Succeeded()) TruckAttackAnimations.Add(PoliceTruckAttackTwo.Object);
	DeathAnimation = PoliceDeathOne.Object;
	DeathAnimations.Reset();
	if (PoliceDeathOne.Succeeded()) DeathAnimations.Add(PoliceDeathOne.Object);
	if (PoliceDeathTwo.Succeeded()) DeathAnimations.Add(PoliceDeathTwo.Object);
}

FVector APoliceZombie::GetCrawlingMeshRelativeLocation(const FVector& CurrentStandingMeshRelativeLocation) const
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

void APoliceZombie::InitializeBoneDurability()
{
	ResetDismemberBones();
	RegisterDismemberBone(TEXT("Head"), 10.0f);
	RegisterDismemberBone(TEXT("LeftArm"), 15.0f);
	RegisterDismemberBone(TEXT("LeftForeArm"), 10.0f);
	RegisterDismemberBone(TEXT("RightArm"), 15.0f);
	RegisterDismemberBone(TEXT("RightForeArm"), 10.0f);
	RegisterDismemberBone(TEXT("LeftUpLeg"), 20.0f);
	RegisterDismemberBone(TEXT("LeftLeg"), 15.0f);
	RegisterDismemberBone(TEXT("RightUpLeg"), 20.0f);
	RegisterDismemberBone(TEXT("RightLeg"), 15.0f);
	RegisterDismemberBone(TEXT("Spine"), 50.0f);
}

FName APoliceZombie::GetParentBoneForDamage(FName HitBoneName) const
{
	const FString Bone = HitBoneName.ToString();
	if (Bone.Contains(TEXT("Head")) || Bone.Contains(TEXT("Neck"))) return TEXT("Head");
	if (Bone.Contains(TEXT("LeftUpLeg"))) return TEXT("LeftUpLeg");
	if (Bone.Contains(TEXT("RightUpLeg"))) return TEXT("RightUpLeg");
	if (Bone.Contains(TEXT("LeftLeg")) || Bone.Contains(TEXT("LeftFoot")) || Bone.Contains(TEXT("LeftToe"))) return TEXT("LeftLeg");
	if (Bone.Contains(TEXT("RightLeg")) || Bone.Contains(TEXT("RightFoot")) || Bone.Contains(TEXT("RightToe"))) return TEXT("RightLeg");
	if (Bone.Contains(TEXT("LeftForeArm")) || Bone.Contains(TEXT("LeftHand"))) return TEXT("LeftForeArm");
	if (Bone.Contains(TEXT("RightForeArm")) || Bone.Contains(TEXT("RightHand"))) return TEXT("RightForeArm");
	if (Bone.Contains(TEXT("LeftArm")) || Bone.Contains(TEXT("LeftShoulder"))) return TEXT("LeftArm");
	if (Bone.Contains(TEXT("RightArm")) || Bone.Contains(TEXT("RightShoulder"))) return TEXT("RightArm");
	if (Bone.Contains(TEXT("Spine")) || Bone.Contains(TEXT("Hips"))) return TEXT("Spine");
	return HitBoneName;
}

FName APoliceZombie::GetPhysicsRootBoneName() const
{
	return TEXT("Hips");
}

bool APoliceZombie::IsFatalDismemberBone(FName BoneName) const
{
	return BoneName == TEXT("Head") || BoneName == TEXT("Spine");
}

bool APoliceZombie::IsLegBone(FName BoneName) const
{
	return BoneName == TEXT("LeftUpLeg") || BoneName == TEXT("LeftLeg") ||
		BoneName == TEXT("RightUpLeg") || BoneName == TEXT("RightLeg");
}
