// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectiles/FPSProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Particles/ParticleSystem.h"
#include "Kismet/GameplayStatics.h"  
#include "Truck/Truck.h"

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
        // ?ㅽ뵾?대? ?⑥닚 肄쒕━???쒗쁽?쇰줈 ?ъ슜
        CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));

        // ?ㅽ뵾?댁쓽 肄쒕━??諛섍꼍???ㅼ젙
        CollisionComponent->InitSphereRadius(1.5f);
        CollisionComponent->BodyInstance.bUseCCD = true;

        // 異⑸룎泥섎━ 梨꾨꼸???깅줉..? 
		CollisionComponent->BodyInstance.SetCollisionProfileName(TEXT("Projectile")); // 肄쒕━???꾨줈?뚯씪 ?ㅼ젙
        CollisionComponent->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);

        // 而댄룷?뚰듃媛 ?대뵖媛??遺?ろ옄 ???몄텧?섎뒗 ?대깽??
        CollisionComponent->OnComponentHit.AddDynamic(this, &AFPSProjectile::OnHit);

        // 猷⑦듃 而댄룷?뚰듃媛 肄쒕━??而댄룷?뚰듃媛 ?섎룄濡??ㅼ젙
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
		ProjectileMovementComponent->ProjectileGravityScale = 0.0f;             // 촣알이 받는 중력
    }

    if (!ProjectileMeshComponent)
    {
        ProjectileMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMeshComponent"));
        static ConstructorHelpers::FObjectFinder<UStaticMesh>Mesh(TEXT("/Script/Engine.StaticMesh'/Game/Projectiles/bullet.bullet'"));
        if (Mesh.Succeeded())
        {
            ProjectileMeshComponent->SetStaticMesh(Mesh.Object);
        }
        //?숈쟻?쇰줈 硫뷀?由ъ뼹 ?곸슜
        static ConstructorHelpers::FObjectFinder<UMaterial>Material(TEXT("/Script/Engine.Material'/Game/Projectiles/M_AK47.M_AK47'"));
        if (Material.Succeeded())
        {
            ProjectileMaterialInstance = UMaterialInstanceDynamic::Create(Material.Object, ProjectileMeshComponent);
        }
        ProjectileMeshComponent->SetMaterial(0, ProjectileMaterialInstance);
        ProjectileMeshComponent->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.08f));
        ProjectileMeshComponent->SetupAttachment(RootComponent);
    }
	// ?쇨꺽 ?댄럺??濡쒕뱶
    if(!StoneImpactEffect)
    {
        static ConstructorHelpers::FObjectFinder<UParticleSystem>ImpactEffect(TEXT("/Script/Engine.ParticleSystem'/Game/MilitaryWeapSilver/FX/P_Impact_Stone_Large_01.P_Impact_Stone_Large_01'"));
        if (ImpactEffect.Succeeded())
        {
            StoneImpactEffect = ImpactEffect.Object;
        }
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
        //諛쒖궗泥댁쓽 ?띾룄媛 ProjectileMovementComponent ???섑빐 ?뺤쓽?섍린 ?뚮Ц??諛쒖궗 諛⑺뼢留??쒓났?섎㈃ ??
        ProjectileMovementComponent->Velocity = ShootDirection * ProjectileMovementComponent->InitialSpeed;
	}
}

void AFPSProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (Cast<ATruck>(OtherActor))
    {
        return;
    }

    // ?먭린 ?먯떊?대굹 諛쒖궗??Owner)???쒖쇅
    AActor* MyOwner = GetOwner();
    AActor* InstigatorActor = GetInstigator();
    if (OtherActor && OtherActor != this && OtherActor != MyOwner && OtherActor != InstigatorActor)
    {

        UGameplayStatics::ApplyPointDamage(
            OtherActor,            // Damage ???(醫鍮?
            20.f,                   // ?곕?吏 媛?
            ProjectileMovementComponent->Velocity.GetSafeNormal(),  // 諛쒖궗 諛⑺뼢(?뱀? ShotDirection)
            Hit,                   // !!! ?ш린???ㅼ젣 異⑸룎 FHitResult ?꾩껜 ?섍?
            GetInstigatorController(),  // 而⑦듃濡ㅻ윭
            this,                  // ?곕?吏 ?뚯뒪(珥앹븣 ?먯떊)
            nullptr                // DamageType
        );

        UE_LOG(LogTemp, Warning, TEXT("ammo damage to %s! "), *GetNameSafe(OtherActor));
    }

	const bool bCanApplyPhysicsImpulse =
		OtherActor != this &&
		OtherComponent &&
		OtherComponent->IsSimulatingPhysics() &&
		!Cast<ATruck>(OtherActor) &&
		!Cast<APawn>(OtherActor);

	if (bCanApplyPhysicsImpulse)  // 차량이나 폰에는 총알 impulse를 주지 않아 튕김 버그를 막는다.
    {
		OtherComponent->AddImpulseAtLocation(ProjectileMovementComponent->Velocity * 100.0f, Hit.ImpactPoint);  // 異⑸룎 吏?먯뿉 諛쒖궗泥댁쓽 ?띾룄??鍮꾨??섎뒗 ?꾪럡?ㅻ? 媛??
    }


    if(StoneImpactEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(),
            StoneImpactEffect,
            Hit.ImpactPoint,
            Hit.ImpactNormal.Rotation());  // ?쇨꺽 ?댄럺???ъ깮
    }
	
	ReturnToPool(); // 異⑸룎 ???濡?諛섑솚
}

//----------------------------------------------------------------------------------------
//  ?留??명꽣?섏씠??援ы쁽 
//----------------------------------------------------------------------------------------

void AFPSProjectile::OnPoolActivate_Implementation()
{
    // ProjectileMovementComponent ?쒖꽦??
    if (ProjectileMovementComponent)
    {
        ProjectileMovementComponent->SetActive(true);
        ProjectileMovementComponent->Velocity = FVector::ZeroVector;
    }

    // ?섎챸 ??대㉧ ?쒖옉
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
    // ??대㉧ ?뺣━
    GetWorld()->GetTimerManager().ClearTimer(LifetimeTimerHandle);

    // ?대룞 ?뺤?
    if (ProjectileMovementComponent)
    {
        ProjectileMovementComponent->StopMovementImmediately();
        ProjectileMovementComponent->SetActive(false);
    }
}

void AFPSProjectile::OnPoolSpawn_Implementation(const FVector& Location, const FRotator& Rotation)
{
    SetActorLocation(Location);
    SetActorRotation(Rotation);

    // ?띾룄 由ъ뀑
    if (ProjectileMovementComponent)
    {
        ProjectileMovementComponent->Velocity = FVector::ZeroVector;
    }
}

void AFPSProjectile::ReturnToPool()
{
    // ??대㉧ ?뺣━
    GetWorld()->GetTimerManager().ClearTimer(LifetimeTimerHandle);

    // Subsystem???듯빐 ???諛섑솚
    if (UWorld* World = GetWorld())
    {
        if (UObjectPoolSubSystem* PoolSubsystem = World->GetSubsystem<UObjectPoolSubSystem>())
        {
            PoolSubsystem->ReturnToPool(this);
            return;
        }
    }

    // ????놁쑝硫??뚭눼
    Destroy();
}

