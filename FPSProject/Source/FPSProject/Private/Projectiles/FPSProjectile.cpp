// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectiles/FPSProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Particles/ParticleSystem.h"
#include "Kismet/GameplayStatics.h"  
#include "Sound/SoundBase.h"
#include "FPSProjectGameInstance.h"
#include "Characters/FPSBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Truck/Truck.h"
#include "Zombie/BaseZombie.h"

namespace
{
FHitResult BuildZombieDamageHit(const FHitResult& Hit, ABaseZombie* HitZombie)
{
	FHitResult DamageHit = Hit;

	if (HitZombie == nullptr || DamageHit.BoneName != NAME_None)
	{
		return DamageHit;
	}

	USkeletalMeshComponent* ZombieMesh = HitZombie->GetMesh();
	if (ZombieMesh == nullptr)
	{
		return DamageHit;
	}

	FVector ClosestBoneLocation = FVector::ZeroVector;
	const FName ClosestBoneName = ZombieMesh->FindClosestBone(Hit.ImpactPoint, &ClosestBoneLocation);
	if (ClosestBoneName != NAME_None)
	{
		DamageHit.BoneName = ClosestBoneName;
		DamageHit.Component = ZombieMesh;
		DamageHit.Location = Hit.ImpactPoint;
		DamageHit.ImpactPoint = Hit.ImpactPoint;
		UE_LOG(LogTemp, Log, TEXT("Projectile inferred zombie bone %s from component %s"),
			*ClosestBoneName.ToString(),
			*GetNameSafe(Hit.GetComponent()));
	}

	return DamageHit;
}

bool IsZombieHeadHit(FName BoneName)
{
	const FString BoneString = BoneName.ToString();
	return BoneString.Contains(TEXT("head"), ESearchCase::IgnoreCase) ||
		BoneString.Contains(TEXT("neck"), ESearchCase::IgnoreCase) ||
		BoneString.Contains(TEXT("eye"), ESearchCase::IgnoreCase);
}
}

// Sets default values
AFPSProjectile::AFPSProjectile()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;


	if (!RootComponent)
	{
		RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ProjectileSceneComponent"));
	}

	if (!CollisionComponent)
	{
		// 스피어 컴포넌트를 단순 콜리전 표현용으로 사용
		CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));

		// 스피어 콜리전 반경 설정
		CollisionComponent->InitSphereRadius(1.5f);
		CollisionComponent->BodyInstance.bUseCCD = true;

		// 충돌 처리 채널 등록
		CollisionComponent->BodyInstance.SetCollisionProfileName(TEXT("Projectile")); // 콜리전 프로파일 설정
		CollisionComponent->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);

		// 컴포넌트가 다른 물체에 부딪히면 호출되는 이벤트
		CollisionComponent->OnComponentHit.AddDynamic(this, &AFPSProjectile::OnHit);

		// 루트 컴포넌트를 콜리전 컴포넌트로 설정
		RootComponent = CollisionComponent;
	}

	if (!ProjectileMovementComponent)
	{
		// 이 컴포넌트를 이용해 발사체 이동 구현
		ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
		ProjectileMovementComponent->SetUpdatedComponent(CollisionComponent);
		ProjectileMovementComponent->InitialSpeed = 20000.0f;                   // 초기 속도
		ProjectileMovementComponent->MaxSpeed = 20000.0f;                       // 최대 속도
		ProjectileMovementComponent->bForceSubStepping = true;
		ProjectileMovementComponent->bRotationFollowsVelocity = true;           // 속도에 따라 회전
		ProjectileMovementComponent->bShouldBounce = false;                     // 바닥에 바운스
		ProjectileMovementComponent->Bounciness = 0.0f;
		ProjectileMovementComponent->ProjectileGravityScale = 0.0f;             // 총알이 받는 중력
	}

	if (!ProjectileMeshComponent)
	{
		ProjectileMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMeshComponent"));
		static ConstructorHelpers::FObjectFinder<UStaticMesh>Mesh(TEXT("/Script/Engine.StaticMesh'/Game/Projectiles/bullet.bullet'"));
		if (Mesh.Succeeded())
		{
			ProjectileMeshComponent->SetStaticMesh(Mesh.Object);
		}
		// 동적으로 머티리얼 적용
		static ConstructorHelpers::FObjectFinder<UMaterial>Material(TEXT("/Script/Engine.Material'/Game/Projectiles/M_AK47.M_AK47'"));
		if (Material.Succeeded())
		{
			ProjectileMaterialInstance = UMaterialInstanceDynamic::Create(Material.Object, ProjectileMeshComponent);
		}
		ProjectileMeshComponent->SetMaterial(0, ProjectileMaterialInstance);
		ProjectileMeshComponent->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.08f));
		ProjectileMeshComponent->SetupAttachment(RootComponent);
	}
	// 충격 이펙트 로드
	if(!StoneImpactEffect)
	{
		static ConstructorHelpers::FObjectFinder<UParticleSystem>ImpactEffect(TEXT("/Script/Engine.ParticleSystem'/Game/MilitaryWeapSilver/FX/P_Impact_Stone_Large_01.P_Impact_Stone_Large_01'"));
		if (ImpactEffect.Succeeded())
		{
			StoneImpactEffect = ImpactEffect.Object;
		}
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> ZombieHeadHitSoundAsset(
		TEXT("/Game/Sound/bulletHit.bulletHit"));
	if (ZombieHeadHitSoundAsset.Succeeded())
	{
		ZombieHeadHitSound = ZombieHeadHitSoundAsset.Object;
	}
}

// Called when the game starts or when spawned
void AFPSProjectile::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AFPSProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AFPSProjectile::FireInDirection(const FVector& ShootDirection)
{
	if (ProjectileMovementComponent)
	{
		FVector SafeDirection = ShootDirection.GetSafeNormal();
		if (SafeDirection.IsNearlyZero())
		{
			SafeDirection = GetActorForwardVector();
		}

		if (CollisionComponent)
		{
			ProjectileMovementComponent->SetUpdatedComponent(CollisionComponent);
		}

		ProjectileMovementComponent->Activate(true);
		ProjectileMovementComponent->Velocity = SafeDirection * ProjectileMovementComponent->InitialSpeed;
		ProjectileMovementComponent->UpdateComponentVelocity();
	}
}

void AFPSProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (Cast<ATruck>(OtherActor))
	{
		return;
	}

	// 자기 자신이나 발사자와의 충돌은 무시
	AActor* MyOwner = GetOwner();
	AActor* InstigatorActor = GetInstigator();
	if (OtherActor && OtherActor != this && OtherActor != MyOwner && OtherActor != InstigatorActor)
	{
		ABaseZombie* HitZombie = Cast<ABaseZombie>(OtherActor);
		const FHitResult DamageHit = BuildZombieDamageHit(Hit, HitZombie);
		if (HitZombie && HitZombie->IsAlive() && IsZombieHeadHit(DamageHit.BoneName) && ZombieHeadHitSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ZombieHeadHitSound, DamageHit.ImpactPoint);
		}

		AFPSBaseCharacter* InstigatorCharacter = Cast<AFPSBaseCharacter>(InstigatorActor);
		const bool bSentZombieHitPacket =
			InstigatorCharacter &&
			UFPSProjectGameInstance::SendZombieHitPacket(
				InstigatorCharacter,
				HitZombie,
				20.0f,
				DamageHit.ImpactPoint,
				DamageHit.BoneName,
				DamageHit.ImpactNormal);


		if (!bSentZombieHitPacket)
		{
			UGameplayStatics::ApplyPointDamage(
				OtherActor,            // 데미지 대상
				20.f,                  // 데미지 값
				ProjectileMovementComponent->Velocity.GetSafeNormal(),  // 발사 방향
				DamageHit,             // 충돌 정보
				GetInstigatorController(),  // 발사자 컨트롤러
				this,                  // 데미지 원인
				nullptr                // DamageType
			);
		}

		UE_LOG(LogTemp, Warning, TEXT("ammo damage to %s! Bone=%s Component=%s"),
			*GetNameSafe(OtherActor),
			*DamageHit.BoneName.ToString(),
			*GetNameSafe(DamageHit.GetComponent()));
	}

	const bool bCanApplyPhysicsImpulse =
		OtherActor != this &&
		OtherComponent &&
		OtherComponent->IsSimulatingPhysics() &&
		!Cast<ATruck>(OtherActor) &&
		!Cast<APawn>(OtherActor);

	if (bCanApplyPhysicsImpulse)  // 차량이나 폰에는 총알 impulse를 주지 않아 튕김 버그를 막는다.
	{
		OtherComponent->AddImpulseAtLocation(ProjectileMovementComponent->Velocity * 100.0f, Hit.ImpactPoint);  // 충돌 지점에 발사체 속도 비례 충격 적용
	}


	if(StoneImpactEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			StoneImpactEffect,
			Hit.ImpactPoint,
			Hit.ImpactNormal.Rotation());  // 충격 이펙트 생성
	}
	
	ReturnToPool(); // 충돌 후 풀로 반환
}

//----------------------------------------------------------------------------------------
//  풀링 인터페이스 구현
//----------------------------------------------------------------------------------------

void AFPSProjectile::OnPoolActivate_Implementation()
{
	// ProjectileMovementComponent 활성화
	if (ProjectileMovementComponent)
	{
		if (CollisionComponent)
		{
			ProjectileMovementComponent->SetUpdatedComponent(CollisionComponent);
		}

		ProjectileMovementComponent->Activate(true);
		ProjectileMovementComponent->Velocity = FVector::ZeroVector;
		ProjectileMovementComponent->UpdateComponentVelocity();
	}

	// 수명 타이머 시작
	GetWorld()->GetTimerManager().SetTimer(
		LifetimeTimerHandle,
		this,
		&AFPSProjectile::ReturnToPool,
		LifetimeSeconds,
		false
	);
}

void AFPSProjectile::OnPoolDeactivate_Implementation()
{
	// 타이머 정리
	GetWorld()->GetTimerManager().ClearTimer(LifetimeTimerHandle);

	// 이동 정지
	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->StopMovementImmediately();
		ProjectileMovementComponent->Deactivate();
	}
}

void AFPSProjectile::OnPoolSpawn_Implementation(const FVector& Location, const FRotator& Rotation)
{
	SetActorLocation(Location);
	SetActorRotation(Rotation);

	if (CollisionComponent)
	{
		CollisionComponent->ClearMoveIgnoreActors();
	}

	// 속도 리셋
	if (ProjectileMovementComponent)
	{
		if (CollisionComponent)
		{
			ProjectileMovementComponent->SetUpdatedComponent(CollisionComponent);
		}

		ProjectileMovementComponent->Velocity = FVector::ZeroVector;
		ProjectileMovementComponent->UpdateComponentVelocity();
	}
}

void AFPSProjectile::ReturnToPool()
{
	// 타이머 정리
	GetWorld()->GetTimerManager().ClearTimer(LifetimeTimerHandle);

	// Subsystem을 통해 풀에 반환
	if (UWorld* World = GetWorld())
	{
		if (UObjectPoolSubSystem* PoolSubsystem = World->GetSubsystem<UObjectPoolSubSystem>())
		{
			PoolSubsystem->ReturnToPool(this);
			return;
		}
	}

	// 풀이 없으면 파괴
	Destroy();
}

