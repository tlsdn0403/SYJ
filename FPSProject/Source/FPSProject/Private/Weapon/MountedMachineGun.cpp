#include "Weapon/MountedMachineGun.h"

#include "Characters/FPSBaseCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Projectiles/FPSProjectile.h"
#include "Sound/SoundBase.h"
#include "Subsystems/ObjectPoolSubSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

AMountedMachineGun::AMountedMachineGun()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	YawPivot = CreateDefaultSubobject<USceneComponent>(TEXT("YawPivot"));
	YawPivot->SetupAttachment(SceneRoot);

	PitchPivot = CreateDefaultSubobject<USceneComponent>(TEXT("PitchPivot"));
	PitchPivot->SetupAttachment(YawPivot);

	GunMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetupAttachment(PitchPivot);
	GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GunMesh->SetSimulatePhysics(false);
	GunMesh->SetEnableGravity(false);

	MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
	MuzzlePoint->SetupAttachment(GunMesh);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(PitchPivot);
	CameraBoom->TargetArmLength = 0.0f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->SocketOffset = CameraSocketOffset;

	CameraPoint = CreateDefaultSubobject<USceneComponent>(TEXT("CameraPoint"));
	CameraPoint->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	static ConstructorHelpers::FClassFinder<UAnimInstance> GunAnimBP(
		TEXT("/Game/Heavy_Machine_Gun/Animation_Blueprints/ABP_Heavy_Machine_Gun"));
	if (GunAnimBP.Succeeded())
	{
		GunAnimationBlueprintClass = GunAnimBP.Class;
	}
}

void AMountedMachineGun::BeginPlay()
{
	Super::BeginPlay();

	if (GunMesh)
	{
		GunMesh->SetSimulatePhysics(false);
		GunMesh->SetEnableGravity(false);
		GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (!GunMesh->GetAnimClass() && GunAnimationBlueprintClass)
		{
			GunMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			GunMesh->SetAnimInstanceClass(GunAnimationBlueprintClass);
		}
	}
}

void AMountedMachineGun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateFireAnimation(DeltaTime);
}

void AMountedMachineGun::SetWeaponUser(AFPSBaseCharacter* NewUser)
{
	CurrentUser = NewUser;
}

FVector AMountedMachineGun::GetCameraLocation() const
{
	return CameraPoint ? CameraPoint->GetComponentLocation() : GetActorLocation();
}

FRotator AMountedMachineGun::GetCameraRotation() const
{
	return CameraPoint ? CameraPoint->GetComponentRotation() : GetActorRotation();
}

void AMountedMachineGun::Fire()
{
	if (!CurrentUser || !ProjectileClass || !GetWorld())
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastFireTime < FireInterval)
	{
		return;
	}
	LastFireTime = CurrentTime;

	const FVector CameraLocation = GetCameraLocation();
	const FRotator CameraRotation = GetCameraRotation();

	const FVector TraceStart = CameraLocation;
	const FVector TraceEnd = TraceStart + CameraRotation.Vector() * 10000.0f;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(CurrentUser);
	if (CurrentUser->CurrentTruck)
	{
		QueryParams.AddIgnoredActor(CurrentUser->CurrentTruck);
	}

	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	FVector TargetLocation = bHit ? HitResult.Location : TraceEnd;

	FVector FireLocation = GetActorLocation();
	if (GunMesh && MuzzleSocketName != NAME_None && GunMesh->DoesSocketExist(MuzzleSocketName))
	{
		FireLocation = GunMesh->GetSocketLocation(MuzzleSocketName);
	}
	else if (MuzzlePoint)
	{
		FireLocation = MuzzlePoint->GetComponentLocation();
	}

	FHitResult MuzzleHitResult;
	FCollisionQueryParams MuzzleQueryParams(SCENE_QUERY_STAT(MountedMachineGunMuzzleTrace), true);
	MuzzleQueryParams.AddIgnoredActor(this);
	MuzzleQueryParams.AddIgnoredActor(CurrentUser);
	if (CurrentUser->CurrentTruck)
	{
		MuzzleQueryParams.AddIgnoredActor(CurrentUser->CurrentTruck);
	}

	const bool bMuzzleBlocked = GetWorld()->LineTraceSingleByChannel(
		MuzzleHitResult,
		FireLocation,
		TargetLocation,
		ECC_Visibility,
		MuzzleQueryParams);

	if (bMuzzleBlocked)
	{
		TargetLocation = MuzzleHitResult.Location;
	}

	const FVector FireDirection = (TargetLocation - FireLocation).GetSafeNormal();
	const FRotator FireRotation = FireDirection.Rotation();

	if (UObjectPoolSubSystem* PoolSubsystem = GetWorld()->GetSubsystem<UObjectPoolSubSystem>())
	{
		if (AActor* PooledActor = PoolSubsystem->SpawnFromPool(ProjectileClass, FireLocation, FireRotation))
		{
				if (AFPSProjectile* Projectile = Cast<AFPSProjectile>(PooledActor))
				{
					Projectile->SetOwner(CurrentUser);
					Projectile->SetInstigator(CurrentUser);
					if (Projectile->CollisionComponent)
					{
						Projectile->CollisionComponent->IgnoreActorWhenMoving(this, true);
						Projectile->CollisionComponent->IgnoreActorWhenMoving(CurrentUser, true);
						if (CurrentUser->CurrentTruck)
						{
							Projectile->CollisionComponent->IgnoreActorWhenMoving(CurrentUser->CurrentTruck, true);
						}
					}
					Projectile->FireInDirection(FireDirection);
				}
			}
	}
	else
	{
		if (AFPSProjectile* Projectile = GetWorld()->SpawnActor<AFPSProjectile>(ProjectileClass, FireLocation, FireRotation))
		{
			Projectile->SetOwner(CurrentUser);
			Projectile->SetInstigator(CurrentUser);
			if (Projectile->CollisionComponent)
			{
				Projectile->CollisionComponent->IgnoreActorWhenMoving(this, true);
				Projectile->CollisionComponent->IgnoreActorWhenMoving(CurrentUser, true);
				if (CurrentUser->CurrentTruck)
				{
					Projectile->CollisionComponent->IgnoreActorWhenMoving(CurrentUser->CurrentTruck, true);
				}
			}
			Projectile->FireInDirection(FireDirection);
		}
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, FireLocation);
	}

	if (GunParticleEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), GunParticleEffect, FireLocation, FireRotation);
	}

	ApplyFireAnimation();
	ApplyMountedRecoil();
}

void AMountedMachineGun::UpdateAim(const FRotator& ControlRotation)
{
	const FRotator ActorRotation = GetActorRotation();
	const float RelativeYaw = FMath::FindDeltaAngleDegrees(ActorRotation.Yaw, ControlRotation.Yaw);
	const float RelativePitch = FMath::ClampAngle(ControlRotation.Pitch, MinPitch, MaxPitch);

	if (YawPivot)
	{
		YawPivot->SetRelativeRotation(FRotator(0.0f, RelativeYaw, 0.0f));
	}

	if (PitchPivot)
	{
		PitchPivot->SetRelativeRotation(FRotator(RelativePitch, 0.0f, 0.0f));
	}

	if (CameraBoom)
	{
		CameraBoom->SocketOffset = CameraSocketOffset;
	}
}

void AMountedMachineGun::ApplyMountedRecoil() const
{
	if (!CurrentUser)
	{
		return;
	}

	const float PitchKick = FMath::FRandRange(RecoilPitchRange.X, RecoilPitchRange.Y);
	const float YawKick = FMath::FRandRange(-RecoilYawMagnitude, RecoilYawMagnitude);
	CurrentUser->ApplyWeaponRecoil(PitchKick, YawKick);
}

void AMountedMachineGun::ApplyFireAnimation()
{
	TriggerAnimationAlpha = 1.0f;
	RecoilAnimationAlpha = 1.0f;
	UpdateFireAnimation(0.0f);
}

void AMountedMachineGun::UpdateFireAnimation(float DeltaTime)
{
	if (!GunMesh)
	{
		return;
	}

	UAnimInstance* AnimInstance = GunMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	if (DeltaTime > 0.0f)
	{
		TriggerAnimationAlpha = FMath::FInterpTo(
			TriggerAnimationAlpha,
			0.0f,
			DeltaTime,
			TriggerAnimationReturnSpeed);

		RecoilAnimationAlpha = FMath::FInterpTo(
			RecoilAnimationAlpha,
			0.0f,
			DeltaTime,
			RecoilAnimationReturnSpeed);
	}

	SetAnimFloatProperty(AnimInstance, TEXT("Anim_Trigger"), TriggerAnimationAlpha);
	SetAnimFloatProperty(AnimInstance, TEXT("Trigger"), TriggerAnimationAlpha);
	SetAnimFloatProperty(AnimInstance, TEXT("Gun_Translation"), RecoilAnimationAlpha);
}

void AMountedMachineGun::SetAnimFloatProperty(UAnimInstance* AnimInstance, const TCHAR* PropertyName, float Value) const
{
	if (!AnimInstance)
	{
		return;
	}

	if (FFloatProperty* FloatProperty = FindFProperty<FFloatProperty>(AnimInstance->GetClass(), PropertyName))
	{
		FloatProperty->SetPropertyValue_InContainer(AnimInstance, Value);
	}
}
