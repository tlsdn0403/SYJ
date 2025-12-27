#include "Weapon/WeaponBase.h"
#include "Projectiles/FPSProjectile.h"
#include "Characters/FPSBaseCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Sound/SoundBase.h"
#include "Engine/Engine.h" // 디버그 메시지 출력용

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

    // 부착할 메쉬 선택 
    USkeletalMeshComponent* AttachMesh = Character->GetMesh();

    // 부착 트랜스폼 설정
    FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
    AttachToComponent(AttachMesh, AttachmentRules, AttachSocketName);

    // 부착 후 무기 Transform(Scale, 위치, 회전) 조정
    WeaponMesh->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.15f));
    /*WeaponMesh->SetRelativeLocation(FVector::ZeroVector);
    WeaponMesh->SetRelativeRotation(FRotator::ZeroRotator);*/
    // 찾으신 위치 값 (X, Y, Z)
    WeaponMesh->SetRelativeLocation(FVector(-35.209697f, 2.353551f, 0.508678f));

    // 찾으신 회전 값 (Pitch, Yaw, Roll 순서)
    WeaponMesh->SetRelativeRotation(FRotator(1.090108f, -88.966904f, -4.015320f));

    // 캐릭터의 CurrentWeapon 업데이트
    Character->CurrentWeapon = this;

    // 애니메이션에서 탄창이 자꾸 보여서 안보이도록 
    if (WeaponMesh)
    {
        // PBO_None: 물리 충돌체 업데이트는 놔두고 시각적으로만 숨김 
        //PBO_Term → 물리 바디(충돌체)까지 완전히 해제
        WeaponMesh->HideBoneByName(TEXT("Bone_002"), PBO_Term);
    }
    // 확장: 입력 바인딩, 애님BP 전환 등 추가 가능
}

void AWeaponBase::DetachWeapon()
{
    // 무기 분리
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Character = nullptr;
    // 확장: 입력 언바인딩, 상태 해제 등 추가 가능
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
            Character->GetActorEyesViewPoint(CameraLocation, CameraRotation);

            FRotator FireRotation = CameraRotation;  // 캐릭터 카메라 방향으로 총알 발사

            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.Instigator = Character;

            AFPSProjectile* Projectile = World->SpawnActor<AFPSProjectile>(ProjectileClass, FireLocation, FireRotation, SpawnParams);

            DrawDebugSphere(GetWorld(), FireLocation, 5.0f, 12, FColor::Red, false, 3.0f);
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