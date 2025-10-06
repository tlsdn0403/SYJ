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

    MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);

    AttachSocketName = TEXT("GripPoint"); // 필요에 따라 소켓명 지정 (BP에서 Override 가능)
}

void AWeaponBase::BeginPlay()
{
    Super::BeginPlay();
}

void AWeaponBase::AttachWeapon(AFPSBaseCharacter* TargetCharacter)
{
    Character = TargetCharacter;
    if (!Character) return;

    // 부착할 메쉬 선택 (여기서는 3인칭 기준, 1인칭이면 GetFirstPersonMesh() 등으로 변경)
    USkeletalMeshComponent* AttachMesh = Character->GetMesh();

    // 부착 트랜스폼 설정
    FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
    AttachToComponent(AttachMesh, AttachmentRules, AttachSocketName);

    // 부착 후 무기 Transform(Scale, 위치, 회전) 조정
    WeaponMesh->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));
    WeaponMesh->SetRelativeLocation(FVector::ZeroVector);
    WeaponMesh->SetRelativeRotation(FRotator::ZeroRotator);

    // 캐릭터의 CurrentWeapon 업데이트
    Character->CurrentWeapon = this;

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
            FVector CameraLocation;
            FRotator CameraRotation;
            Character->GetActorEyesViewPoint(CameraLocation, CameraRotation);

            const FVector SpawnLocation = CameraLocation + CameraRotation.RotateVector(MuzzleOffset);
            const FRotator SpawnRotation = CameraRotation;

            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.Instigator = Character;

            AFPSProjectile* Projectile = World->SpawnActor<AFPSProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
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