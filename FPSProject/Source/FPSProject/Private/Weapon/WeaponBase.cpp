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

    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;

    AttachSocketName = TEXT("Gun_socket");
}

void AWeaponBase::BeginPlay()
{
    Super::BeginPlay();

    // 게임 인스턴스를 찾아서 나를 등록함
    if (auto* GI = Cast<UFPSProjectGameInstance>(GetGameInstance()))
    {
        if (ObjectId != 0) // ID가 설정된 경우에만
        {
            GI->FieldItems.Add(ObjectId, this);
        }
    }
}

void AWeaponBase::AttachWeapon(AFPSBaseCharacter* TargetCharacter)
{
    Character = TargetCharacter;
    if (!Character) return;

    USkeletalMeshComponent* AttachMesh = Character->GetMesh();

    FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
    AttachToComponent(AttachMesh, AttachmentRules, AttachSocketName);

    WeaponMesh->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.15f));
    WeaponMesh->SetRelativeLocation(FVector(-35.209697f, 2.353551f, 0.508678f));
    WeaponMesh->SetRelativeRotation(FRotator(1.090108f, -88.966904f, -4.015320f));

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

            FVector CameraLocation;
            FRotator CameraRotation;
            Character->GetWeaponAimViewPoint(CameraLocation, CameraRotation);

            FVector TraceStart = CameraLocation;
            FVector TraceEnd = TraceStart + CameraRotation.Vector() * 10000.0f;

            FHitResult HitResult;
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(this);

            const bool bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
            const FVector TargetLocation = bHit ? HitResult.Location : TraceEnd;

            FVector FireDirection = (TargetLocation - FireLocation).GetSafeNormal();
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

float AWeaponBase::GetCurrentSpreadAngleDegrees() const
{
    if (!Character)
    {
        return HipFireSpreadAngleDegrees;
    }

    return Character->IsAiming() ? AimSpreadAngleDegrees : HipFireSpreadAngleDegrees;
}

