#include "Weapon/MountedMachineGun.h"

#include "Characters/FPSBaseCharacter.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Projectiles/FPSProjectile.h"
#include "Sound/SoundBase.h"
#include "Subsystems/ObjectPoolSubSystem.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/UnrealType.h"
#include "Truck/Truck.h"

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

	MuzzlePoint = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzlePoint"));
	MuzzlePoint->SetupAttachment(GunMesh);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(PitchPivot);
	CameraBoom->TargetArmLength = 0.0f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraLagSpeed = CameraLocationLagSpeed;
	CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;
	CameraBoom->SocketOffset = CameraSocketOffset;

	CameraPoint = CreateDefaultSubobject<USceneComponent>(TEXT("CameraPoint"));
	CameraPoint->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	MagazineActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("MagazineActorComponent"));
	MagazineActorComponent->SetupAttachment(GunMesh);
	MagazineActorComponent->SetRelativeLocation(FVector::ZeroVector);

	CurrentBulletsInMagazine = MagazineCapacity;
}

void AMountedMachineGun::BeginPlay()
{
	Super::BeginPlay();

	if (GunMesh && MountedGunAnimClass)
	{
		GunMesh->SetAnimInstanceClass(MountedGunAnimClass);
	}

	if (MagazineActorComponent && MagazineActorClass)
	{
		MagazineActorComponent->SetChildActorClass(MagazineActorClass);
	}

	CurrentBulletsInMagazine = MagazineCapacity;
	UpdateMagazineState(false);
}

void AMountedMachineGun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateAnimationState(DeltaTime);
	UpdateMagazineState(false);
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
	CurrentSlideOffset = SlideKickDistance;
	CurrentTriggerValue = TriggerPressedValue;
	bFireInputActive = true;
	CurrentBulletsInMagazine = FMath::Max(0, CurrentBulletsInMagazine - 1);

	FVector CameraLocation;
	FRotator CameraRotation;
	CurrentUser->GetActorEyesViewPoint(CameraLocation, CameraRotation);

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
	const FVector TargetLocation = bHit ? HitResult.Location : TraceEnd;

	FVector FireLocation = GetActorLocation();
	if (GunMesh && MuzzleSocketName != NAME_None && GunMesh->DoesSocketExist(MuzzleSocketName))
	{
		FireLocation = GunMesh->GetSocketLocation(MuzzleSocketName);
	}
	else if (MuzzlePoint)
	{
		FireLocation = MuzzlePoint->GetComponentLocation();
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

	UpdateMagazineState(true);
}

void AMountedMachineGun::UpdateAim(const FRotator& ControlRotation)
{
	const FRotator ActorRotation = GetActorRotation();
	TargetRelativeYaw = FMath::Clamp(FMath::FindDeltaAngleDegrees(ActorRotation.Yaw, ControlRotation.Yaw), MinYaw, MaxYaw);
	TargetRelativePitch = FMath::ClampAngle(ControlRotation.Pitch, MinPitch, MaxPitch);

	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	CurrentRelativeYaw = FMath::FInterpTo(CurrentRelativeYaw, TargetRelativeYaw, DeltaSeconds, YawInterpSpeed);
	CurrentRelativePitch = FMath::FInterpTo(CurrentRelativePitch, TargetRelativePitch, DeltaSeconds, PitchInterpSpeed);

	if (YawPivot)
	{
		YawPivot->SetRelativeRotation(FRotator(0.0f, CurrentRelativeYaw, 0.0f));
	}

	if (PitchPivot)
	{
		PitchPivot->SetRelativeRotation(FRotator(CurrentRelativePitch, 0.0f, 0.0f));
	}

	if (CameraBoom)
	{
		CameraBoom->CameraLagSpeed = CameraLocationLagSpeed;
		CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;
		CameraBoom->SocketOffset = CameraSocketOffset;
	}

	SetAnimRotatorValue(TEXT("Left_Right_Direction_Part_Rotation"), FRotator(0.0f, CurrentRelativeYaw * HorizontalBoneYawMultiplier, 0.0f));
	SetAnimRotatorValue(TEXT("Gun_Up_Down_Direction_Rotation"), FRotator(CurrentRelativePitch * VerticalBonePitchMultiplier, 0.0f, 0.0f));
	SetAnimVectorValue(TEXT("Gun_Translation"), GunTranslationOffset);
}

void AMountedMachineGun::UpdateAnimationState(float DeltaTime)
{
	CurrentSlideOffset = FMath::FInterpTo(CurrentSlideOffset, 0.0f, DeltaTime, SlideReturnSpeed);
	CurrentTriggerValue = FMath::FInterpTo(CurrentTriggerValue, 0.0f, DeltaTime, SlideReturnSpeed * 2.0f);
	bFireInputActive = (GetWorld() && (GetWorld()->GetTimeSeconds() - LastFireTime) <= (FireInterval * 1.5f));

	SetAnimFloatValue(TEXT("Anim_Slide"), CurrentSlideOffset);
	SetAnimFloatValue(TEXT("Anim_Trigger"), CurrentTriggerValue);
}

void AMountedMachineGun::UpdateMagazineState(bool bTriggeredByFire)
{
	SetChildActorIntValue(MagazineActorComponent, MagazineBulletCountPropertyName, CurrentBulletsInMagazine);
	SetChildActorBoolValue(MagazineActorComponent, MagazineFirePressedPropertyName, bFireInputActive);
	SetChildActorBoolValue(MagazineActorComponent, MagazineSystemWorkingPropertyName, bFireInputActive);

	if (bTriggeredByFire)
	{
		CallChildActorFunction(MagazineActorComponent, MagazineFireEventName);
		CallChildActorFunction(MagazineActorComponent, MagazineSetBulletCountFunctionName);
	}
}

void AMountedMachineGun::SetAnimFloatValue(FName PropertyName, float Value) const
{
	if (!GunMesh)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GunMesh->GetAnimInstance())
	{
		if (FFloatProperty* FloatProperty = FindFProperty<FFloatProperty>(AnimInstance->GetClass(), PropertyName))
		{
			FloatProperty->SetPropertyValue_InContainer(AnimInstance, Value);
		}
	}
}

void AMountedMachineGun::SetAnimVectorValue(FName PropertyName, const FVector& Value) const
{
	if (!GunMesh)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GunMesh->GetAnimInstance())
	{
		if (FStructProperty* StructProperty = FindFProperty<FStructProperty>(AnimInstance->GetClass(), PropertyName))
		{
			if (StructProperty->Struct == TBaseStructure<FVector>::Get())
			{
				*StructProperty->ContainerPtrToValuePtr<FVector>(AnimInstance) = Value;
			}
		}
	}
}

void AMountedMachineGun::SetAnimRotatorValue(FName PropertyName, const FRotator& Value) const
{
	if (!GunMesh)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GunMesh->GetAnimInstance())
	{
		if (FStructProperty* StructProperty = FindFProperty<FStructProperty>(AnimInstance->GetClass(), PropertyName))
		{
			if (StructProperty->Struct == TBaseStructure<FRotator>::Get())
			{
				*StructProperty->ContainerPtrToValuePtr<FRotator>(AnimInstance) = Value;
			}
		}
	}
}

void AMountedMachineGun::SetChildActorIntValue(UChildActorComponent* ChildActorComponent, FName PropertyName, int32 Value) const
{
	if (!ChildActorComponent)
	{
		return;
	}

	if (AActor* ChildActor = ChildActorComponent->GetChildActor())
	{
		if (FIntProperty* IntProperty = FindFProperty<FIntProperty>(ChildActor->GetClass(), PropertyName))
		{
			IntProperty->SetPropertyValue_InContainer(ChildActor, Value);
		}
	}
}

void AMountedMachineGun::SetChildActorBoolValue(UChildActorComponent* ChildActorComponent, FName PropertyName, bool Value) const
{
	if (!ChildActorComponent)
	{
		return;
	}

	if (AActor* ChildActor = ChildActorComponent->GetChildActor())
	{
		if (FBoolProperty* BoolProperty = FindFProperty<FBoolProperty>(ChildActor->GetClass(), PropertyName))
		{
			BoolProperty->SetPropertyValue_InContainer(ChildActor, Value);
		}
	}
}

bool AMountedMachineGun::CallChildActorFunction(UChildActorComponent* ChildActorComponent, FName FunctionName) const
{
	if (!ChildActorComponent || FunctionName.IsNone())
	{
		return false;
	}

	if (AActor* ChildActor = ChildActorComponent->GetChildActor())
	{
		if (UFunction* Function = ChildActor->FindFunction(FunctionName))
		{
			ChildActor->ProcessEvent(Function, nullptr);
			return true;
		}
	}

	return false;
}

