// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/WeaponBase.h"
#include"Projectiles/FPSProjectile.h"
#include "Characters/FPSBaseCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Sound/SoundBase.h"

// Sets default values
AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	RootComponent = WeaponMesh;

	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);
}

// Called when the game starts or when spawned
void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
}

void AWeaponBase::AttachWeapon(AFPSBaseCharacter* TargetCharacter)
{
	Character = TargetCharacter;
	if (!Character)
	{
		return;
	}

	// Attach to first person mesh's socket (can be changed as needed)
	USkeletalMeshComponent* AttachMesh = Character->GetMesh(); // 기본: 3인칭 바디
	if (Character->FindComponentByClass<UCameraComponent>())
	{
		// FPS의 경우, 1인칭 메시에 붙이고 싶으면 따로 처리 필요
		// AttachMesh = Character->GetFirstPersonMesh(); // 직접 구현 필요
	}
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(AttachMesh, AttachmentRules, AttachSocketName);
}

void AWeaponBase::Fire()
{
	if (!Character || !Character->GetController()) return;

	if (ProjectileClass)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			// 카메라 위치/방향을 얻음 (1인칭/3인칭 모두 일관)
			FVector CameraLocation;
			FRotator CameraRotation;
			Character->GetActorEyesViewPoint(CameraLocation, CameraRotation);

			// MuzzleOffset은 카메라 기준
			const FVector SpawnLocation = CameraLocation + CameraRotation.RotateVector(MuzzleOffset);
			const FRotator SpawnRotation = CameraRotation;

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.Instigator = Character;

			AFPSProjectile* Projectile = World->SpawnActor<AFPSProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
			// 필요시 발사자 정보 추가 전달
		}
	}

	// Fire sound
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
	}

	// Fire animation
	if (FireAnimation)
	{
		USkeletalMeshComponent* AnimMesh = Character->GetMesh(); // 3인칭 Mesh
		// 1인칭 FPS라면 Character에 GetFirstPersonMesh() 구현해서 사용 가능
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

