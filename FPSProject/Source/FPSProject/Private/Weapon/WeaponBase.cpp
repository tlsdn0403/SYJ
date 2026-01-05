#include "Weapon/WeaponBase.h"
#include "Projectiles/FPSProjectile.h"
#include "Characters/FPSBaseCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Sound/SoundBase.h"
#include "Particles/ParticleSystem.h"
#include "Subsystems/ObjectPoolSubSystem.h"
#include "Engine/Engine.h" // 디버그 메시지 출력용

bool bDebug = false;
AWeaponBase::AWeaponBase()
{
    PrimaryActorTick.bCanEverTick = true;

    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;


    AttachSocketName = TEXT("Gun_socket"); // 필요에 따라 소켓명 지정
}

void AWeaponBase::BeginPlay()
{
    Super::BeginPlay();
}

void AWeaponBase::AttachWeapon(AFPSBaseCharacter* TargetCharacter)
{
    Character = TargetCharacter;
    if (!Character) return;

    USkeletalMeshComponent* AttachMesh = Character->GetMesh();                          // 부착할 메쉬 선택 



	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);     // 부착 규칙 설정
    AttachToComponent(AttachMesh, AttachmentRules, AttachSocketName);

   
    WeaponMesh->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.15f));                       // 부착 후 무기 Transform(Scale, 위치, 회전) 조정

    //캐릭터의 올바른 위치에 총기 부착되도록
    WeaponMesh->SetRelativeLocation(FVector(-35.209697f, 2.353551f, 0.508678f));      // 찾은 위치 값 (X, Y, Z)
    WeaponMesh->SetRelativeRotation(FRotator(1.090108f, -88.966904f, -4.015320f));    // 찾은 회전 값 (Pitch, Yaw, Roll 순서)


    Character->CurrentWeapon = this;                                                     // 캐릭터의 CurrentWeapon 업데이트


    // 애니메이션에서 탄창이 자꾸 보여서 안보이도록 
    if (WeaponMesh)
    {
        // PBO_None: 물리 충돌체 업데이트는 놔두고 시각적으로만 숨김 
        //PBO_Term → 물리 바디(충돌체)까지 완전히 해제
        WeaponMesh->HideBoneByName(TEXT("Bone_002"), PBO_Term);
    }

}

void AWeaponBase::DetachWeapon()
{
    // 무기 분리
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Character = nullptr;

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


            // 카메라 위치/방향 얻기
            FVector CameraLocation;
            FRotator CameraRotation;
            Character->GetActorEyesViewPoint(CameraLocation, CameraRotation);

            // 카메라 방향으로 Line Trace 
            FVector TraceStart = CameraLocation;
            FVector TraceEnd = TraceStart + CameraRotation.Vector() * 10000.0f;                                     // 10,000cm 거리까지 트레이스

            FHitResult HitResult;
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(this);                                                                      // 자기 캐릭터 무시

            bool bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

          
            FVector TargetLocation = bHit ? HitResult.Location : TraceEnd;                                          // 라인 트레이스 했을 때 히트한다면 히트한 위치 , 아니라면 최대 거리 위치로 총알을발사


            //  총구에서 타겟 위치로 향하는 방향
            FVector FireDirection = (TargetLocation - FireLocation).GetSafeNormal();
            FRotator FireRotation = FireDirection.Rotation();


            // ===== 풀링 사용 =====
            UObjectPoolSubSystem* PoolSubsystem = World->GetSubsystem<UObjectPoolSubSystem>();
            if (PoolSubsystem)
            {
                AActor* PooledActor = PoolSubsystem->SpawnFromPool(
                    ProjectileClass, FireLocation, FireRotation);

                if (AFPSProjectile* Projectile = Cast<AFPSProjectile>(PooledActor))
                {
                    Projectile->SetOwner(this);
                    Projectile->SetInstigator(Character);
                    Projectile->FireInDirection(FireDirection);
                }
            }
			// ====================

            if (bDebug) {
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
            UAnimInstance* AnimInstance = AnimMesh->GetAnimInstance();
            if (AnimInstance)
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