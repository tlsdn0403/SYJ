#include "Zombie/MixamoZombie.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "AIController.h"
#include "Truck/Truck.h"
#include "UObject/ConstructorHelpers.h"

AMixamoZombie::AMixamoZombie()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.05f;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> DefaultMesh(
		TEXT("/Script/Engine.SkeletalMesh'/Game/Zombie/mixamo/ch/zom_ch10/Ch10_nonPBR.Ch10_nonPBR'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> TeamIdle(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/ayjstart_Anim.ayjstart_Anim'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DefaultWalk(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/Walking__3_.Walking__3_'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DefaultRun(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/Zombie_Run.Zombie_Run'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DefaultCrawl(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/Zombie_Crawl.Zombie_Crawl'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> DefaultCrawlingAttack(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/Zombie_Attack.Zombie_Attack'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> TeamAttackOne(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/standing_attack-zom_Anim.standing_attack-zom_Anim'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> TeamAttackTwo(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/standing_attack2-zom1_Anim.standing_attack2-zom1_Anim'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> TeamAttackThree(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/standing_attack3_Anim.standing_attack3_Anim'"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> TeamDeath(
		TEXT("/Script/Engine.AnimSequence'/Game/Zombie/mixamo/Ani/NewFolder/Zombie_Death.Zombie_Death'"));
	static ConstructorHelpers::FClassFinder<AAIController> DefaultAIController(
		TEXT("/Game/Zombie/AI/BP_AIZombieController"));

	if (DefaultMesh.Succeeded() && GetMesh())
	{
		GetMesh()->SetSkeletalMesh(DefaultMesh.Object);
	}
	IdleAnimation = TeamIdle.Object;
	WalkAnimation = DefaultWalk.Object;
	RunAnimation = DefaultRun.Object;
	CrawlAnimation = DefaultCrawl.Object;
	CrawlingAttackAnimation = DefaultCrawlingAttack.Object;
	DeathAnimation = TeamDeath.Object;
	if (TeamAttackOne.Succeeded())
	{
		AttackAnimations.Add(TeamAttackOne.Object);
	}
	if (TeamAttackTwo.Succeeded())
	{
		AttackAnimations.Add(TeamAttackTwo.Object);
	}
	if (TeamAttackThree.Succeeded())
	{
		AttackAnimations.Add(TeamAttackThree.Object);
	}
	if (DefaultAIController.Succeeded())
	{
		AIControllerClass = DefaultAIController.Class;
	}
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AMixamoZombie::BeginPlay()
{
	Super::BeginPlay();

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!bUseDirectAnimation || !MeshComp)
	{
		SetActorTickEnabled(false);
		return;
	}

	MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	CurrentDirectState = EDirectAnimationState::None;
	UpdateDirectAnimation();
	SetActorTickEnabled(true);
}

void AMixamoZombie::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateDirectAnimation();
}

void AMixamoZombie::OnNetworkMoveAnimationUpdated()
{
	UpdateDirectAnimation();
}

void AMixamoZombie::UpdateDirectAnimation()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!bUseDirectAnimation || !MeshComp || MeshComp->IsSimulatingPhysics())
	{
		return;
	}

	EDirectAnimationState DesiredState = EDirectAnimationState::Idle;
	if (!IsAlive())
	{
		DesiredState = EDirectAnimationState::Dead;
	}
	else if (IsAttacking())
	{
		DesiredState = IsCrawling()
			? EDirectAnimationState::CrawlingAttacking
			: EDirectAnimationState::Attacking;
	}
	else if (IsCrawling())
	{
		DesiredState = EDirectAnimationState::Crawling;
	}
	else
	{
		const float NetworkSpeed2D = GetNetworkAnimationMoveSpeed2D();
		const float Speed2D = NetworkSpeed2D > KINDA_SMALL_NUMBER ? NetworkSpeed2D : GetVelocity().Size2D();
		const float SpeedSquared2D = FMath::Square(Speed2D);
		if (SpeedSquared2D > FMath::Square(FMath::Max(RunningSpeedThreshold, MovingSpeedThreshold)))
		{
			DesiredState = EDirectAnimationState::Running;
		}
		else if (SpeedSquared2D > FMath::Square(MovingSpeedThreshold))
		{
			DesiredState = EDirectAnimationState::Walking;
		}
	}

	if (DesiredState != CurrentDirectState)
	{
		PlayDirectAnimation(DesiredState);
	}
}

void AMixamoZombie::PlayDirectAnimation(EDirectAnimationState NewState)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	if (NewState == EDirectAnimationState::Attacking && !CurrentAttackAnimation)
	{
		ChooseAttackAnimation();
	}

	UAnimSequenceBase* Animation = GetAnimationForState(NewState);
	bool bFreezeFallbackPose = false;
	if (!Animation && NewState == EDirectAnimationState::Idle)
	{
		Animation = WalkAnimation;
		bFreezeFallbackPose = true;
	}

	if (!Animation || !IsAnimationCompatible(Animation))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("MixamoZombie %s cannot play animation %s for state %d. Mesh=%s Speed=%.2f NetworkSpeed=%.2f"),
			*GetName(),
			*GetNameSafe(Animation),
			static_cast<int32>(NewState),
			*GetNameSafe(MeshComp->GetSkeletalMeshAsset()),
			GetVelocity().Size2D(),
			GetNetworkAnimationMoveSpeed2D());
		return;
	}

	const bool bLoop = NewState == EDirectAnimationState::Idle ||
		NewState == EDirectAnimationState::Walking ||
		NewState == EDirectAnimationState::Running ||
		NewState == EDirectAnimationState::Crawling;
	MeshComp->PlayAnimation(Animation, bLoop);

	if (bFreezeFallbackPose)
	{
		if (UAnimSingleNodeInstance* SingleNode = MeshComp->GetSingleNodeInstance())
		{
			SingleNode->SetPosition(0.0f, false);
			SingleNode->SetPlaying(false);
		}
	}

	CurrentDirectState = NewState;
	if (NewState != EDirectAnimationState::Attacking &&
		NewState != EDirectAnimationState::CrawlingAttacking)
	{
		CurrentAttackAnimation = nullptr;
	}
}

UAnimSequenceBase* AMixamoZombie::GetAnimationForState(EDirectAnimationState State) const
{
	switch (State)
	{
	case EDirectAnimationState::Idle:
		return IdleAnimation;
	case EDirectAnimationState::Walking:
		return WalkAnimation;
	case EDirectAnimationState::Running:
		return RunAnimation ? RunAnimation.Get() : WalkAnimation.Get();
	case EDirectAnimationState::Crawling:
		return CrawlAnimation ? CrawlAnimation.Get() : WalkAnimation.Get();
	case EDirectAnimationState::Attacking:
		return CurrentAttackAnimation;
	case EDirectAnimationState::CrawlingAttacking:
		return CrawlingAttackAnimation ? CrawlingAttackAnimation.Get() : CurrentAttackAnimation.Get();
	case EDirectAnimationState::Dead:
		return CurrentDeathAnimation ? CurrentDeathAnimation.Get() : DeathAnimation.Get();
	default:
		return nullptr;
	}
}

UAnimSequenceBase* AMixamoZombie::ChooseAttackAnimation()
{
	const bool bAttackingTruck = IsValid(GetCurrentAttackTarget()) && GetCurrentAttackTarget()->IsA<ATruck>();
	const TArray<TObjectPtr<UAnimSequenceBase>>& CandidateSource =
		bAttackingTruck && !TruckAttackAnimations.IsEmpty()
			? TruckAttackAnimations
			: AttackAnimations;

	TArray<UAnimSequenceBase*> CompatibleAnimations;
	for (UAnimSequenceBase* Animation : CandidateSource)
	{
		if (Animation && IsAnimationCompatible(Animation))
		{
			CompatibleAnimations.Add(Animation);
		}
	}

	if (CompatibleAnimations.IsEmpty())
	{
		CurrentAttackAnimation = nullptr;
		return nullptr;
	}

	CurrentAttackAnimation = CompatibleAnimations[FMath::RandRange(0, CompatibleAnimations.Num() - 1)];
	return CurrentAttackAnimation;
}

UAnimSequenceBase* AMixamoZombie::ChooseDeathAnimation()
{
	TArray<UAnimSequenceBase*> CompatibleAnimations;
	for (UAnimSequenceBase* Animation : DeathAnimations)
	{
		if (Animation && IsAnimationCompatible(Animation))
		{
			CompatibleAnimations.Add(Animation);
		}
	}

	if (CompatibleAnimations.IsEmpty())
	{
		CurrentDeathAnimation = IsAnimationCompatible(DeathAnimation) ? DeathAnimation.Get() : nullptr;
		return CurrentDeathAnimation;
	}

	CurrentDeathAnimation = CompatibleAnimations[FMath::RandRange(0, CompatibleAnimations.Num() - 1)];
	return CurrentDeathAnimation;
}

bool AMixamoZombie::IsAnimationCompatible(const UAnimSequenceBase* Animation) const
{
	const USkeletalMeshComponent* MeshComp = GetMesh();
	const USkeletalMesh* MeshAsset = MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr;
	const USkeleton* AnimationSkeleton = Animation ? Animation->GetSkeleton() : nullptr;
	return AnimationSkeleton && MeshAsset && AnimationSkeleton->IsCompatibleMesh(MeshAsset);
}

float AMixamoZombie::GetAnimationDuration(const UAnimSequenceBase* Animation) const
{
	if (!Animation)
	{
		return 0.0f;
	}

	const USkeletalMeshComponent* MeshComp = GetMesh();
	const float RateScale = MeshComp ? FMath::Max(KINDA_SMALL_NUMBER, MeshComp->GlobalAnimRateScale) : 1.0f;
	return Animation->GetPlayLength() / RateScale;
}

float AMixamoZombie::GetDirectAttackAnimationDuration()
{
	UAnimSequenceBase* Animation = IsCrawling() && CrawlingAttackAnimation
		? CrawlingAttackAnimation.Get()
		: ChooseAttackAnimation();
	return IsAnimationCompatible(Animation) ? GetAnimationDuration(Animation) : 0.0f;
}

float AMixamoZombie::PlayDeathAnimationBeforeRagdoll()
{
	UAnimSequenceBase* SelectedDeathAnimation = ChooseDeathAnimation();
	if (!bUseDirectAnimation || !SelectedDeathAnimation)
	{
		return 0.0f;
	}

	PlayDirectAnimation(EDirectAnimationState::Dead);
	return GetAnimationDuration(SelectedDeathAnimation);
}

void AMixamoZombie::InitializeBoneDurability()
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

FName AMixamoZombie::GetParentBoneForDamage(FName HitBoneName) const
{
	const FString Bone = HitBoneName.ToString();
	const FString LowerBone = Bone.ToLower();
	if (LowerBone == TEXT("head")) return TEXT("Head");
	if (LowerBone == TEXT("upperarm_l")) return TEXT("LeftArm");
	if (LowerBone == TEXT("lowerarm_l")) return TEXT("LeftForeArm");
	if (LowerBone == TEXT("upperarm_r")) return TEXT("RightArm");
	if (LowerBone == TEXT("lowerarm_r")) return TEXT("RightForeArm");
	if (LowerBone == TEXT("thigh_l")) return TEXT("LeftUpLeg");
	if (LowerBone == TEXT("calf_l")) return TEXT("LeftLeg");
	if (LowerBone == TEXT("thigh_r")) return TEXT("RightUpLeg");
	if (LowerBone == TEXT("calf_r")) return TEXT("RightLeg");
	if (LowerBone == TEXT("spine_01")) return TEXT("Spine");

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

FName AMixamoZombie::GetPhysicsRootBoneName() const
{
	return TEXT("Hips");
}

bool AMixamoZombie::IsFatalDismemberBone(FName BoneName) const
{
	return BoneName == TEXT("Head") || BoneName == TEXT("Spine");
}

bool AMixamoZombie::IsLegBone(FName BoneName) const
{
	return BoneName == TEXT("LeftUpLeg") || BoneName == TEXT("LeftLeg") ||
		BoneName == TEXT("RightUpLeg") || BoneName == TEXT("RightLeg");
}