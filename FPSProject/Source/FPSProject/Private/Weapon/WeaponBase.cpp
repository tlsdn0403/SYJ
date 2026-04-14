#include "Weapon/WeaponBase.h"
#include "FPSProjectGameInstance.h" 
#include "Weapon/WeaponBase.h"
#include "Projectiles/FPSProjectile.h"
#include "Characters/FPSBaseCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Sound/SoundBase.h"
#include "Particles/ParticleSystem.h"
#include "Subsystems/ObjectPoolSubSystem.h"
#include "Engine/Engine.h"
#include "Math/UnrealMathUtility.h"

bool bDebug = false;
AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);

	AttachSocketName = TEXT("Gun_socket");
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
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
	WeaponMesh->SetRelativeLocation(FVector(-7.40821f, 4.648937f, 1.158742f));
	WeaponMesh->SetRelativeRotation(FRotator(-6.316770f, -264.543091f, 2.009403f));

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
}

void AWeaponBase::SetWeaponCollisionEnabled(bool bEnabled)
{
	SetActorEnableCollision(bEnabled);

	if (WeaponMesh)
	{
		WeaponMesh->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void AWeaponBase::SetWeaponHidden(bool Hidden)
{
	SetActorHiddenInGame(Hidden);

	if (WeaponMesh)
	{
		WeaponMesh->SetVisibility(!Hidden, true);
	}
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

			FVector CameraLocation;
			FRotator CameraRotation;
			Character->GetWeaponAimViewPoint(CameraLocation, CameraRotation);

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

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Blue, TEXT("빵! (상대방 화면에서 가짜 총알 발사됨!)"));
	}
	UE_LOG(LogTemp, Warning, TEXT("[Network] RemoteFire 실행됨! 남의 총구에서 쐈습니다."));

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

