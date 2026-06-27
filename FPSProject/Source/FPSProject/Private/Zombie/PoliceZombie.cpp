// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/PoliceZombie.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
FName GetPoliceDismemberRootBone(FName HitBoneName)
{
	const FString Bone = HitBoneName.ToString();
	const FString LowerBone = Bone.ToLower();

	if (LowerBone == TEXT("head"))
	{
		return TEXT("Head");
	}

	if (LowerBone == TEXT("upperarm_l"))
	{
		return TEXT("LeftArm");
	}

	if (LowerBone == TEXT("lowerarm_l"))
	{
		return TEXT("LeftForeArm");
	}

	if (LowerBone == TEXT("upperarm_r"))
	{
		return TEXT("RightArm");
	}

	if (LowerBone == TEXT("lowerarm_r"))
	{
		return TEXT("RightForeArm");
	}

	if (LowerBone == TEXT("thigh_l"))
	{
		return TEXT("LeftUpLeg");
	}

	if (LowerBone == TEXT("calf_l"))
	{
		return TEXT("LeftLeg");
	}

	if (LowerBone == TEXT("thigh_r"))
	{
		return TEXT("RightUpLeg");
	}

	if (LowerBone == TEXT("calf_r"))
	{
		return TEXT("RightLeg");
	}

	if (LowerBone == TEXT("spine_01"))
	{
		return TEXT("Spine");
	}

	if (Bone == TEXT("Head") ||
		Bone == TEXT("HeadTop_End") ||
		Bone == TEXT("Neck") ||
		Bone == TEXT("LeftEye") ||
		Bone == TEXT("RightEye"))
	{
		return TEXT("Head");
	}

	if (Bone == TEXT("LeftShoulder") || Bone == TEXT("LeftArm"))
	{
		return TEXT("LeftArm");
	}

	if (Bone == TEXT("LeftForeArm") || Bone.StartsWith(TEXT("LeftHand")))
	{
		return TEXT("LeftForeArm");
	}

	if (Bone == TEXT("RightShoulder") || Bone == TEXT("RightArm"))
	{
		return TEXT("RightArm");
	}

	if (Bone == TEXT("RightForeArm") || Bone.StartsWith(TEXT("RightHand")))
	{
		return TEXT("RightForeArm");
	}

	if (Bone == TEXT("LeftUpLeg"))
	{
		return TEXT("LeftUpLeg");
	}

	if (Bone == TEXT("LeftLeg") ||
		Bone == TEXT("LeftFoot") ||
		Bone == TEXT("LeftToeBase") ||
		Bone == TEXT("LeftToe_End"))
	{
		return TEXT("LeftLeg");
	}

	if (Bone == TEXT("RightUpLeg"))
	{
		return TEXT("RightUpLeg");
	}

	if (Bone == TEXT("RightLeg") ||
		Bone == TEXT("RightFoot") ||
		Bone == TEXT("RightToeBase") ||
		Bone == TEXT("RightToe_End"))
	{
		return TEXT("RightLeg");
	}

	if (Bone == TEXT("Hips") ||
		Bone == TEXT("Spine") ||
		Bone == TEXT("Spine1") ||
		Bone == TEXT("Spine2"))
	{
		return TEXT("Spine");
	}

	return NAME_None;
}
}

APoliceZombie::APoliceZombie()
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> PoliceMesh(
		TEXT("/Script/Engine.SkeletalMesh'/Game/Zombie/mixamo/ch/zoM_police/copzombie_l_actisdato.copzombie_l_actisdato'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PoliceIdle(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/police/Idle.Idle'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PoliceWalk(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/police/Walking__2_.Walking__2_'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PoliceRun(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/police/Zombie_Run.Zombie_Run'"));
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
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	}

	if (PoliceIdle.Succeeded()) IdleAnimation = PoliceIdle.Object;
	if (PoliceWalk.Succeeded()) WalkAnimation = PoliceWalk.Object;
	if (PoliceRun.Succeeded()) RunAnimation = PoliceRun.Object;
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

void APoliceZombie::BeginPlay()
{
	if (UAnimSequenceBase* PoliceIdle = LoadObject<UAnimSequenceBase>(nullptr,
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/police/Idle.Idle'")))
	{
		IdleAnimation = PoliceIdle;
	}
	if (UAnimSequenceBase* PoliceWalk = LoadObject<UAnimSequenceBase>(nullptr,
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/police/Walking__2_.Walking__2_'")))
	{
		WalkAnimation = PoliceWalk;
	}
	if (UAnimSequenceBase* PoliceRun = LoadObject<UAnimSequenceBase>(nullptr,
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/police/Zombie_Run.Zombie_Run'")))
	{
		RunAnimation = PoliceRun;
	}

	Super::BeginPlay();

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Capsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		MeshComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	}
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
	if (const FName MappedBone = GetPoliceDismemberRootBone(HitBoneName); MappedBone != NAME_None)
	{
		return MappedBone;
	}

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