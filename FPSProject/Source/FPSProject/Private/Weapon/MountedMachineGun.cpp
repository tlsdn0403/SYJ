#include "Weapon/MountedMachineGun.h"

#include "Characters/FPSBaseCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Projectiles/FPSProjectile.h"
#include "Sound/SoundBase.h"
#include "Subsystems/ObjectPoolSubSystem.h"
#include "Truck/Truck.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
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

	FeedBulletComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FeedBulletComponent"));
	FeedBulletComponent->SetupAttachment(GunMesh);
	FeedBulletComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FeedBulletComponent->SetCastShadow(false);
	FeedBulletComponent->SetVisibility(false);

	MagazineActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("MagazineActorComponent"));
	MagazineActorComponent->SetupAttachment(GunMesh);
	MagazineActorComponent->SetRelativeLocation(FVector::ZeroVector);

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

	static ConstructorHelpers::FClassFinder<AActor> EmptyShellBP(
		TEXT("/Game/Heavy_Machine_Gun/Attachments/EmptyShell/Blueprints/BP_EmptyShell"));
	if (EmptyShellBP.Succeeded())
	{
		EmptyShellClass = EmptyShellBP.Class;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BulletMeshAsset(
		TEXT("/Game/Heavy_Machine_Gun/Attachments/Bullet/Static_Meshes/SM_Bullet.SM_Bullet"));
	if (BulletMeshAsset.Succeeded())
	{
		FeedBulletMesh = BulletMeshAsset.Object;
	}

	static ConstructorHelpers::FClassFinder<AActor> MagazineBP(
		TEXT("/Game/Heavy_Machine_Gun/Attachments/Magazine/Blueprints/BP_Magazine"));
	if (MagazineBP.Succeeded())
	{
		MagazineActorClass = MagazineBP.Class;
	}

	CurrentBulletsInMagazine = MagazineCapacity;
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

	if (FeedBulletComponent)
	{
		FeedBulletComponent->SetVisibility(false);
		FeedBulletComponent->SetRelativeScale3D(AmmoFeedBulletScale);
		if (FeedBulletMesh)
		{
			FeedBulletComponent->SetStaticMesh(FeedBulletMesh);
		}
	}

	if (MagazineActorComponent && MagazineActorClass)
	{
		MagazineActorComponent->SetChildActorClass(MagazineActorClass);
	}

	CurrentBulletsInMagazine = MagazineCapacity;
	UpdateMagazineAnimationState(false);
}

void AMountedMachineGun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// 애니메이션 업데이트
	UpdateFireAnimation(DeltaTime);
	UpdateAmmoFeedAnimation(DeltaTime);
	UpdateMagazineAnimationState(false);
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
	bMagazineFirePressed = true;
	CurrentBulletsInMagazine = FMath::Max(0, CurrentBulletsInMagazine - 1);
	MagazineAnimationPlayingTime = CurrentTime + FireInterval;

	const FVector CameraLocation = GetCameraLocation();
	const FRotator CameraRotation = GetCameraRotation();

	const FVector AimDirection = CameraRotation.Vector().GetSafeNormal();
	const FVector TraceStart = CameraLocation;
	const FVector TraceEnd = TraceStart + AimDirection * 10000.0f;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(CurrentUser);
	if (CurrentUser->CurrentTruck)
	{
		QueryParams.AddIgnoredActor(CurrentUser->CurrentTruck);
	}

	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	FVector TargetLocation = bHit ? HitResult.ImpactPoint : TraceEnd;
	AActor* AimHitActor = bHit ? HitResult.GetActor() : nullptr;

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
		const bool bHitSameAimActor = AimHitActor && MuzzleHitResult.GetActor() == AimHitActor;
		if (!bHitSameAimActor)
		{
			TargetLocation = MuzzleHitResult.ImpactPoint;
		}
	}

	FVector FireDirection = (TargetLocation - FireLocation).GetSafeNormal();
	if (FireDirection.IsNearlyZero() || FVector::DotProduct(FireDirection, AimDirection) <= 0.0f)
	{
		FireDirection = AimDirection;
	}

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

	SpawnEmptyShell();
	ApplyFireAnimation();
	StartAmmoFeedAnimation();
	UpdateMagazineAnimationState(true);
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
	// 알파값들 1로
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
	// 애니메이션 인스턴스를 가져옴
	UAnimInstance* AnimInstance = GunMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}
	//알파값을 0으로 보간
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

	// 보간한 값을 애니메이션 블루프린트에서 사용할 수 있도록 애니메이션 인스턴스의 프로퍼티로 설정
	SetAnimFloatProperty(AnimInstance, TEXT("Anim_Trigger"), TriggerAnimationAlpha);
	SetAnimFloatProperty(AnimInstance, TEXT("Trigger"), TriggerAnimationAlpha);
	SetAnimVectorProperty(AnimInstance, TEXT("Gun_Translation"), GunRecoilTranslation * RecoilAnimationAlpha);
}

void AMountedMachineGun::StartAmmoFeedAnimation()
{
	AmmoFeedAnimationAlpha = 1.0f;
	UpdateAmmoFeedAnimation(0.0f);
}

void AMountedMachineGun::UpdateAmmoFeedAnimation(float DeltaTime)
{
	if (!FeedBulletComponent || !GunMesh)
	{
		return;
	}

	if (AmmoFeedAnimationAlpha <= 0.0f)
	{
		FeedBulletComponent->SetVisibility(false);
		return;
	}

	if (DeltaTime > 0.0f && AmmoFeedAnimationDuration > KINDA_SMALL_NUMBER)
	{
		AmmoFeedAnimationAlpha = FMath::Max(
			AmmoFeedAnimationAlpha - (DeltaTime / AmmoFeedAnimationDuration),
			0.0f);
	}

	const bool bHasStartSocket =
		AmmoFeedStartSocketName != NAME_None && GunMesh->DoesSocketExist(AmmoFeedStartSocketName);
	const bool bHasEndSocket =
		AmmoFeedEndSocketName != NAME_None && GunMesh->DoesSocketExist(AmmoFeedEndSocketName);

	const FVector StartLocation = bHasStartSocket
		? GunMesh->GetSocketLocation(AmmoFeedStartSocketName)
		: GunMesh->GetComponentTransform().TransformPosition(AmmoFeedStartOffset);
	const FVector EndLocation = bHasEndSocket
		? GunMesh->GetSocketLocation(AmmoFeedEndSocketName)
		: GunMesh->GetComponentTransform().TransformPosition(AmmoFeedEndOffset);

	const FVector Direction = (EndLocation - StartLocation).GetSafeNormal();
	const FVector BulletLocation = FMath::Lerp(EndLocation, StartLocation, AmmoFeedAnimationAlpha);
	const FRotator BulletRotation = Direction.IsNearlyZero()
		? GunMesh->GetComponentRotation()
		: Direction.Rotation();

	FeedBulletComponent->SetVisibility(true);
	FeedBulletComponent->SetWorldLocationAndRotation(BulletLocation, BulletRotation);

	if (AmmoFeedAnimationAlpha <= 0.0f)
	{
		FeedBulletComponent->SetVisibility(false);
	}
}

void AMountedMachineGun::UpdateMagazineAnimationState(bool bTriggeredByFire)
{
	if (!GetWorld())
	{
		return;
	}

	if (!bTriggeredByFire)
	{
		bMagazineFirePressed = (GetWorld()->GetTimeSeconds() - LastFireTime) <= (FireInterval * 1.5f);
	}

	SetChildActorIntProperty(MagazineActorComponent, MagazineBulletCountPropertyName, CurrentBulletsInMagazine);
	SetChildActorFloatProperty(MagazineActorComponent, MagazineAnimationPlayingTimePropertyName, MagazineAnimationPlayingTime);
	SetChildActorFloatProperty(MagazineActorComponent, MagazineFiringSpeedPropertyName, FireInterval);
	SetChildActorBoolProperty(MagazineActorComponent, MagazineFirePressedPropertyName, bMagazineFirePressed);
	SetChildActorBoolProperty(MagazineActorComponent, MagazineSystemWorkingPropertyName, bMagazineFirePressed);

	if (bTriggeredByFire)
	{
		CallChildActorFunction(MagazineActorComponent, MagazineFireEventName);
	}
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

void AMountedMachineGun::SetAnimVectorProperty(UAnimInstance* AnimInstance, const TCHAR* PropertyName, const FVector& Value) const
{
	if (!AnimInstance)
	{
		return;
	}
	//"Gun_Translation" 프로퍼티 있는지 찾음
	// FVector가 언리얼 내부적으로 struct라서 FStructProperty로 찾음
	if (FStructProperty* StructProperty = FindFProperty<FStructProperty>(AnimInstance->GetClass(), PropertyName))
	{
		if (StructProperty->Struct == TBaseStructure<FVector>::Get())
		{
			void* StructValuePtr = StructProperty->ContainerPtrToValuePtr<void>(AnimInstance);
			*static_cast<FVector*>(StructValuePtr) = Value;
		}
	}
}

void AMountedMachineGun::SetChildActorIntProperty(UChildActorComponent* ChildActorComponent, FName PropertyName, int32 Value) const
{
	if (!ChildActorComponent || PropertyName.IsNone())
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

void AMountedMachineGun::SetChildActorFloatProperty(UChildActorComponent* ChildActorComponent, FName PropertyName, float Value) const
{
	if (!ChildActorComponent || PropertyName.IsNone())
	{
		return;
	}

	if (AActor* ChildActor = ChildActorComponent->GetChildActor())
	{
		if (FFloatProperty* FloatProperty = FindFProperty<FFloatProperty>(ChildActor->GetClass(), PropertyName))
		{
			FloatProperty->SetPropertyValue_InContainer(ChildActor, Value);
		}
	}
}

void AMountedMachineGun::SetChildActorBoolProperty(UChildActorComponent* ChildActorComponent, FName PropertyName, bool Value) const
{
	if (!ChildActorComponent || PropertyName.IsNone())
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

void AMountedMachineGun::SpawnEmptyShell()
{
	if (!EmptyShellClass || !GunMesh || !GetWorld() || !GunMesh->DoesSocketExist(EmptyShellSocketName))
	{
		return;
	}
	// 소켓 위치에서 스폰하도록 소켓 위치 가져와줌
	const FTransform SocketTransform = GunMesh->GetSocketTransform(EmptyShellSocketName, RTS_World);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = CurrentUser;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedShell = GetWorld()->SpawnActor<AActor>(
		EmptyShellClass,
		SocketTransform.GetLocation(),
		SocketTransform.Rotator(),
		SpawnParams);

	if (!SpawnedShell)
	{
		return;
	}

	SpawnedShell->SetOwner(this);
	SpawnedShell->SetInstigator(CurrentUser);
	ResetEmptyShellPhysics(SpawnedShell, false);
	SpawnedShell->SetActorLocationAndRotation(
		SocketTransform.GetLocation(),
		SocketTransform.Rotator(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	ResetEmptyShellPhysics(SpawnedShell, true);

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	SpawnedShell->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	const FVector WorldImpulse =
		SocketTransform.GetRotation().RotateVector(EmptyShellEjectImpulse);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		PrimitiveComponent->IgnoreActorWhenMoving(this, true);
		if (CurrentUser)
		{
			PrimitiveComponent->IgnoreActorWhenMoving(CurrentUser, true);
			if (CurrentUser->CurrentTruck)
			{
				PrimitiveComponent->IgnoreActorWhenMoving(CurrentUser->CurrentTruck, true);
			}
		}

		if (PrimitiveComponent->IsSimulatingPhysics())
		{
			PrimitiveComponent->AddImpulse(WorldImpulse, NAME_None, true);
		}
	}

	SpawnedShell->SetLifeSpan(EmptyShellLifetime);
}

void AMountedMachineGun::ResetEmptyShellPhysics(AActor* ShellActor, bool bEnablePhysics) const
{
	if (!ShellActor)
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	ShellActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		PrimitiveComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
		PrimitiveComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

		if (PrimitiveComponent->CanEditSimulatePhysics())
		{
			PrimitiveComponent->SetSimulatePhysics(bEnablePhysics);
			if (bEnablePhysics)
			{
				PrimitiveComponent->WakeAllRigidBodies();
			}
		}
	}
}

void AMountedMachineGun::ReturnEmptyShellToPool(AActor* ShellActor) const
{
	if (!ShellActor || !GetWorld())
	{
		return;
	}

	ResetEmptyShellPhysics(ShellActor, false);

	if (UObjectPoolSubSystem* PoolSubsystem = GetWorld()->GetSubsystem<UObjectPoolSubSystem>())
	{
		PoolSubsystem->ReturnToPool(ShellActor);
	}
}

