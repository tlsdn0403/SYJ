#include "Weapon/WeaponBase.h"
#include "FPSProjectGameInstance.h" 
#include "Weapon/WeaponBase.h"
#include "Projectiles/FPSProjectile.h"
#include "Characters/FPSBaseCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Sound/SoundBase.h"
#include "Particles/ParticleSystem.h"
#include "Subsystems/ObjectPoolSubSystem.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/Skeleton.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/ConstructorHelpers.h"

bool bDebug = false;

namespace
{
	const FVector CharacterTwoWeaponMeshRelativeLocation(-0.965218f, -1.088177f, 0.471725f);
	const FRotator CharacterTwoWeaponMeshRelativeRotation(45.649582f, 16.242712f, 17.986916f);
	const FVector CharacterTwoWeaponMeshRelativeScale(1.0f, 1.0f, 1.0f);

	const FVector CharacterThreeWeaponMeshRelativeLocation(-0.634522f, -2.488086f, 0.703186f);
	const FRotator CharacterThreeWeaponMeshRelativeRotation(26.268092f, 6.162865f, 15.621803f);
	const FVector CharacterThreeWeaponMeshRelativeScale(1.0f, 1.0f, 1.0f);

	bool UsesCharacterTwoWeaponMeshOffset(const AFPSBaseCharacter* TargetCharacter)
	{
		if (!TargetCharacter)
		{
			return false;
		}

		if (const UClass* CharacterClass = TargetCharacter->GetClass())
		{
			const FString CharacterClassPath = CharacterClass->GetPathName();
			if (CharacterClassPath.Contains(TEXT("BP_FPSBaseCharacter2")))
			{
				return true;
			}
		}

		const USkeletalMeshComponent* TargetMesh = TargetCharacter->GetMesh();
		const USkeletalMesh* TargetSkeletalMesh = TargetMesh ? TargetMesh->GetSkeletalMeshAsset() : nullptr;
		return TargetSkeletalMesh &&
			(TargetSkeletalMesh->GetPathName().Contains(TEXT("/Sarah/")) ||
			 TargetSkeletalMesh->GetName().Contains(TEXT("SK_Sarah")));
	}

	bool UsesCharacterThreeWeaponMeshOffset(const AFPSBaseCharacter* TargetCharacter)
	{
		if (!TargetCharacter)
		{
			return false;
		}

		if (const UClass* CharacterClass = TargetCharacter->GetClass())
		{
			if (CharacterClass->GetPathName().Contains(TEXT("BP_FPSBaseCharacter3")))
			{
				return true;
			}
		}

		const USkeletalMeshComponent* TargetMesh = TargetCharacter->GetMesh();
		const USkeletalMesh* TargetSkeletalMesh = TargetMesh ? TargetMesh->GetSkeletalMeshAsset() : nullptr;
		return TargetSkeletalMesh &&
			(TargetSkeletalMesh->GetPathName().Contains(TEXT("/QuantumCharacter/")) ||
			 TargetSkeletalMesh->GetName().Contains(TEXT("SKM_QuantumCharacter")));
	}

	void ApplyCharacterWeaponMeshOffset(USkeletalMeshComponent* TargetWeaponMesh, const AFPSBaseCharacter* TargetCharacter)
	{
		if (!TargetWeaponMesh)
		{
			return;
		}

		if (UsesCharacterThreeWeaponMeshOffset(TargetCharacter))
		{
			TargetWeaponMesh->SetRelativeLocation(CharacterThreeWeaponMeshRelativeLocation);
			TargetWeaponMesh->SetRelativeRotation(CharacterThreeWeaponMeshRelativeRotation);
			TargetWeaponMesh->SetRelativeScale3D(CharacterThreeWeaponMeshRelativeScale);
			return;
		}

		if (UsesCharacterTwoWeaponMeshOffset(TargetCharacter))
		{
			TargetWeaponMesh->SetRelativeLocation(CharacterTwoWeaponMeshRelativeLocation);
			TargetWeaponMesh->SetRelativeRotation(CharacterTwoWeaponMeshRelativeRotation);
			TargetWeaponMesh->SetRelativeScale3D(CharacterTwoWeaponMeshRelativeScale);
			return;
		}
	}
}

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);

	FirstPersonWeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonWeaponMesh"));
	FirstPersonWeaponMesh->SetupAttachment(RootComponent);
	FirstPersonWeaponMesh->SetOnlyOwnerSee(true);
	FirstPersonWeaponMesh->SetOwnerNoSee(false);
	FirstPersonWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonWeaponMesh->bCastDynamicShadow = false;
	FirstPersonWeaponMesh->CastShadow = false;
	FirstPersonWeaponMesh->SetHiddenInGame(true);
	FirstPersonWeaponMesh->SetVisibility(false, true);

	AttachSocketName = TEXT("Gun_socket");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ReferenceCharacterMesh(
		TEXT("/Game/Man/Mesh/Full/SK_Man_Full_04.SK_Man_Full_04"));
	if (ReferenceCharacterMesh.Succeeded())
	{
		ThirdPersonAlignmentReferenceMesh = ReferenceCharacterMesh.Object;
	}
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	SyncFirstPersonWeaponMesh();
}

void AWeaponBase::SyncFirstPersonWeaponMesh()
{
	if (!IsValid(WeaponMesh) || !IsValid(FirstPersonWeaponMesh))
	{
		return;
	}

	if (USkeletalMesh* SourceMesh = WeaponMesh->GetSkeletalMeshAsset())
	{
		FirstPersonWeaponMesh->SetSkeletalMesh(SourceMesh);
	}

	const int32 MaterialCount = WeaponMesh->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		FirstPersonWeaponMesh->SetMaterial(MaterialIndex, WeaponMesh->GetMaterial(MaterialIndex));
	}

	FirstPersonWeaponMesh->HideBoneByName(TEXT("Bone_002"), PBO_Term);
}

void AWeaponBase::AlignFirstPersonAimPoint(UCameraComponent* AttachCamera)
{
	if (!bAutoAlignFirstPersonAimPoint || !IsValid(AttachCamera) ||
		!IsValid(FirstPersonWeaponMesh) || FirstPersonAimPointSocketName.IsNone() ||
		!FirstPersonWeaponMesh->DoesSocketExist(FirstPersonAimPointSocketName))
	{
		return;
	}

	FirstPersonWeaponMesh->UpdateComponentToWorld();

	const FVector AimPointWorldLocation =
		FirstPersonWeaponMesh->GetSocketLocation(FirstPersonAimPointSocketName);
	const FVector AimPointCameraLocation =
		AttachCamera->GetComponentTransform().InverseTransformPosition(AimPointWorldLocation);

	FVector AlignedLocation = FirstPersonWeaponMesh->GetRelativeLocation();
	AlignedLocation.Y += FirstPersonAimPointCameraOffset.X - AimPointCameraLocation.Y;
	AlignedLocation.Z += FirstPersonAimPointCameraOffset.Y - AimPointCameraLocation.Z;
	FirstPersonWeaponMesh->SetRelativeLocation(AlignedLocation);
}

void AWeaponBase::SetFirstPersonViewEnabled(bool bEnabled, UCameraComponent* AttachCamera)
{
	const bool bShouldEnable = bEnabled && IsValid(AttachCamera);
	bFirstPersonViewEnabled = bShouldEnable;

	if (Character)
	{
		SetOwner(Character);
	}

	if (IsValid(WeaponMesh))
	{
		WeaponMesh->SetOwnerNoSee(bShouldEnable);
	}

	if (!IsValid(FirstPersonWeaponMesh))
	{
		return;
	}

	if (bShouldEnable)
	{
		SyncFirstPersonWeaponMesh();
		FirstPersonWeaponMesh->AttachToComponent(AttachCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		FirstPersonWeaponMesh->SetRelativeLocation(FirstPersonWeaponRelativeLocation);
		FirstPersonWeaponMesh->SetRelativeRotation(FirstPersonWeaponRelativeRotation);
		FirstPersonWeaponMesh->SetRelativeScale3D(FirstPersonWeaponRelativeScale);
		AlignFirstPersonAimPoint(AttachCamera);
		FirstPersonWeaponMesh->SetHiddenInGame(false);
		FirstPersonWeaponMesh->SetVisibility(true, true);
	}
	else
	{
		FirstPersonWeaponMesh->SetHiddenInGame(true);
		FirstPersonWeaponMesh->SetVisibility(false, true);
		FirstPersonWeaponMesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	}
}
void AWeaponBase::AttachWeapon(AFPSBaseCharacter* TargetCharacter)
{
	Character = TargetCharacter;
	if (!Character) return;

	USkeletalMeshComponent* AttachMesh = Character->GetMesh();

	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(AttachMesh, AttachmentRules, AttachSocketName);

	/*WeaponMesh->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.15f));
	WeaponMesh->SetRelativeLocation(FVector(-35.209697f, 2.353551f, 0.508678f));
	WeaponMesh->SetRelativeRotation(FRotator(1.090108f, -88.966904f, -4.015320f));*/

	// 부착 후 무기  위치, 회전 조정
	ApplyCharacterWeaponMeshOffset(WeaponMesh, Character);

	Character->SetCurrentWeapon(this);

	if (WeaponMesh)
	{
		WeaponMesh->HideBoneByName(TEXT("Bone_002"), PBO_Term);
	}
}

void AWeaponBase::DetachWeapon()
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character = nullptr;
}

void AWeaponBase::SetWeaponUser(AFPSBaseCharacter* NewCharacter)
{
	Character = NewCharacter;
	if (NewCharacter)
	{
		SetOwner(NewCharacter);
		AlignThirdPersonWeaponToReference(NewCharacter);
	}
}

void AWeaponBase::AlignThirdPersonWeaponToReference(AFPSBaseCharacter* TargetCharacter)
{
	if (!TargetCharacter || !ThirdPersonAlignmentReferenceMesh)
	{
		return;
	}

	USkeletalMeshComponent* TargetMesh = TargetCharacter->GetMesh();
	USkeleton* ReferenceSkeleton = ThirdPersonAlignmentReferenceMesh->GetSkeleton();
	const USkeletalMeshSocket* ReferenceSocket = ReferenceSkeleton
		? ReferenceSkeleton->FindSocket(AttachSocketName)
		: nullptr;
	if (!TargetMesh || !ReferenceSocket)
	{
		return;
	}

	const FName AttachBoneName = ReferenceSocket->BoneName;
	if (AttachBoneName.IsNone() || TargetMesh->GetBoneIndex(AttachBoneName) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[WeaponAlignment] Character %s has no reference hand bone '%s'."),
			*GetNameSafe(TargetCharacter),
			*AttachBoneName.ToString());
		return;
	}

	// Rebuild the original FPSBaseCharacter socket transform on the target mesh's
	// matching hand bone. This also works when the target skeleton has no Gun_socket
	// (Sarah) or has a differently-authored Gun_socket (Quantum character).
	const FTransform ReferenceSocketTransform = ReferenceSocket->GetSocketLocalTransform();
	const FTransform WeaponFineTune(
		ThirdPersonWeaponRelativeRotation,
		ThirdPersonWeaponRelativeLocation,
		ThirdPersonWeaponRelativeScale);
	const FTransform CombinedRelativeTransform = WeaponFineTune * ReferenceSocketTransform;

	AttachToComponent(
		TargetMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		AttachBoneName);
	SetActorRelativeTransform(CombinedRelativeTransform);

	if (WeaponMesh)
	{
		ApplyCharacterWeaponMeshOffset(WeaponMesh, TargetCharacter);
	}
}

void AWeaponBase::SetWeaponCollisionEnabled(bool bEnabled)
{
	if (!IsValid(this))
	{
		return;
	}

	SetActorEnableCollision(bEnabled);

	if (IsValid(WeaponMesh))
	{
		WeaponMesh->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void AWeaponBase::SetWeaponHidden(bool Hidden)
{
	if (!IsValid(this))
	{
		return;
	}

	SetActorHiddenInGame(Hidden);

	if (IsValid(WeaponMesh))
	{
		WeaponMesh->SetVisibility(!Hidden, true);
	}
}

bool AWeaponBase::GetAimSocketViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	if (!IsValid(WeaponMesh))
	{
		return false;
	}

	// 소켓이 존재하면 소켓 위치와 회전을 반환
	if (AimCameraSocketName != NAME_None && WeaponMesh->DoesSocketExist(AimCameraSocketName))
	{
		OutLocation = WeaponMesh->GetSocketLocation(AimCameraSocketName);
		OutRotation = WeaponMesh->GetSocketRotation(AimCameraSocketName);
		return true;
	}

	return false;
}

bool AWeaponBase::GetAimCameraViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	if (!IsValid(WeaponMesh))
	{
		return false;
	}

	if (GetAimSocketViewPoint(OutLocation, OutRotation))
	{
		return true;
	}

	const FVector BaseLocation = WeaponMesh->DoesSocketExist(MuzzleSocketName)
		? WeaponMesh->GetSocketLocation(MuzzleSocketName)
		: WeaponMesh->GetComponentLocation();

	OutLocation =
		BaseLocation +
		WeaponMesh->GetForwardVector() * AimCameraFallbackOffset.X +
		WeaponMesh->GetRightVector() * AimCameraFallbackOffset.Y +
		WeaponMesh->GetUpVector() * AimCameraFallbackOffset.Z;
	OutRotation = WeaponMesh->GetForwardVector().Rotation();
	return true;
}

void AWeaponBase::Fire()
{
	if (!Character || !Character->GetController()) return;
	UE_LOG(LogTemp, Log, TEXT("WeaponBase::Fire (Shooter: %s, Weapon: %s)"), *GetNameSafe(Character), *GetName());

	if (ProjectileClass)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FVector FireLocation = WeaponMesh->GetComponentLocation() +
				WeaponMesh->GetForwardVector() * MuzzleOffset.X +
				WeaponMesh->GetRightVector() * MuzzleOffset.Y +
				WeaponMesh->GetUpVector() * MuzzleOffset.Z;

			if (WeaponMesh && WeaponMesh->DoesSocketExist(MuzzleSocketName))
			{
				FireLocation = WeaponMesh->GetSocketLocation(MuzzleSocketName);
			}

			FVector CameraLocation = FVector::ZeroVector;
			FRotator CameraRotation = FRotator::ZeroRotator;
			const bool bUseAimSocketTrace =
				bUseAimSocketForIronSightTrace &&
				Character->IsIronSightAiming() &&
				!bFirstPersonViewEnabled &&
				GetAimSocketViewPoint(CameraLocation, CameraRotation);
			if (!bUseAimSocketTrace)
			{
				Character->GetWeaponAimViewPoint(CameraLocation, CameraRotation);
			}

			const FVector AimDirection = CameraRotation.Vector().GetSafeNormal();
			FVector TraceStart = CameraLocation;
			FVector TraceEnd = TraceStart + AimDirection * AimTraceDistance;

			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);
			QueryParams.AddIgnoredActor(Character);

			const bool bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
			FVector TargetLocation = bHit ? HitResult.ImpactPoint : TraceEnd;
			AActor* AimHitActor = bHit ? HitResult.GetActor() : nullptr;

			FHitResult MuzzleHitResult;
			FCollisionQueryParams MuzzleQueryParams;
			MuzzleQueryParams.AddIgnoredActor(this);
			MuzzleQueryParams.AddIgnoredActor(Character);

			const bool bMuzzleBlocked = World->LineTraceSingleByChannel(
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

			const float SpreadAngleDegrees = GetCurrentSpreadAngleDegrees();
			if (SpreadAngleDegrees > 0.0f)
			{
				FireDirection = FMath::VRandCone(
					FireDirection,
					FMath::DegreesToRadians(SpreadAngleDegrees));
			}

			const FRotator FireRotation = FireDirection.Rotation();

			UObjectPoolSubSystem* PoolSubsystem = World->GetSubsystem<UObjectPoolSubSystem>();
			if (PoolSubsystem)
			{
				AActor* PooledActor = PoolSubsystem->SpawnFromPool(ProjectileClass, FireLocation, FireRotation);

				if (AFPSProjectile* Projectile = Cast<AFPSProjectile>(PooledActor))
				{
					Projectile->SetOwner(this);
					Projectile->SetInstigator(Character);
					if (Projectile->CollisionComponent)
					{
						Projectile->CollisionComponent->IgnoreActorWhenMoving(this, true);
						Projectile->CollisionComponent->IgnoreActorWhenMoving(Character, true);
					}
					Projectile->FireInDirection(FireDirection);
				}
			}

			if (bDebug)
			{
				DrawDebugSphere(GetWorld(), FireLocation, 5.0f, 12, FColor::Red, false, 3.0f);
			}
		}
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
	}

	if (FireAnimation)
	{
		USkeletalMeshComponent* AnimMesh = Character->GetMesh();
		if (AnimMesh)
		{
			if (UAnimInstance* AnimInstance = AnimMesh->GetAnimInstance())
			{
				AnimInstance->Montage_Play(FireAnimation, 1.f);
			}
		}
	}

	PlayMuzzleFlash();
	
}

void AWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AWeaponBase::RemoteFire()
{
	if (!Character) return;

	// 1. 소리 & 애니메이션 재생 (기존 Fire에서 복붙)
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
	}
	if (FireAnimation)
	{
		if (USkeletalMeshComponent* AnimMesh = Character->GetMesh())
		{
			if (UAnimInstance* AnimInstance = AnimMesh->GetAnimInstance())
				AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}

	PlayMuzzleFlash();

	// 2. 총알 스폰 (카메라 조준선 계산 없이, 캐릭터가 바라보는 방향으로 무지성 발사!)
	if (ProjectileClass)
	{
		UWorld* World = GetWorld();
		if (World && WeaponMesh && WeaponMesh->DoesSocketExist(MuzzleSocketName))
		{
			FVector FireLocation = WeaponMesh->GetSocketLocation(MuzzleSocketName);
			FRotator FireRotation = Character->GetActorRotation(); // 남의 캐릭터가 보는 방향

			UObjectPoolSubSystem* PoolSubsystem = World->GetSubsystem<UObjectPoolSubSystem>();
			if (PoolSubsystem)
			{
				AActor* PooledActor = PoolSubsystem->SpawnFromPool(ProjectileClass, FireLocation, FireRotation);
				if (AFPSProjectile* Projectile = Cast<AFPSProjectile>(PooledActor))
				{
					// 남의 총알은 나랑 충돌하지 않게 무시 처리
					if (Projectile->CollisionComponent)
						Projectile->CollisionComponent->IgnoreActorWhenMoving(Character, true);

					Projectile->FireInDirection(FireRotation.Vector());
				}
			}
		}
	}
}

float AWeaponBase::GetCurrentSpreadAngleDegrees() const
{
	if (!Character)
	{
		return HipFireSpreadAngleDegrees;
	}

	return Character->IsAiming() ? AimSpreadAngleDegrees : HipFireSpreadAngleDegrees;
}

void AWeaponBase::ApplyFireRecoil() const
{
	if (!Character)
	{
		return;
	}

	const bool bIsAimFire = Character->IsAiming();
	const FVector2D PitchRange = bIsAimFire ? AimRecoilPitchRange : HipFireRecoilPitchRange;
	const float YawMagnitude = bIsAimFire ? AimRecoilYawMagnitude : HipFireRecoilYawMagnitude;

	const float PitchKick = FMath::FRandRange(PitchRange.X, PitchRange.Y);
	const float YawKick = FMath::FRandRange(-YawMagnitude, YawMagnitude);
	Character->ApplyWeaponRecoil(PitchKick, YawKick);
}

void AWeaponBase::PlayMuzzleFlash() const
{
	if (!GunParticleEffect || !WeaponMesh)
	{
		return;
	}

	if (WeaponMesh->DoesSocketExist(MuzzleSocketName))
	{
		UGameplayStatics::SpawnEmitterAttached(
			GunParticleEffect,
			WeaponMesh,
			MuzzleSocketName,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true);
		return;
	}

	UGameplayStatics::SpawnEmitterAtLocation(
		this,
		GunParticleEffect,
		WeaponMesh->GetComponentLocation(),
		WeaponMesh->GetComponentRotation());
}
