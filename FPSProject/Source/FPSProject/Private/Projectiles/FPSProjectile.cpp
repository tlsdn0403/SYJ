// Fill out your copyright notice in the Description page of Project Settings.


#include "Projectiles/FPSProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystem.h"
#include "Kismet/GameplayStatics.h"  

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
        // 스피어를 단순 콜리전 표현으로 사용
        CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));

        // 스피어의 콜리전 반경을 설정
        CollisionComponent->InitSphereRadius(15.0f);

        // 충돌처리 채널에 등록..? 
		CollisionComponent->BodyInstance.SetCollisionProfileName(TEXT("Projectile")); // 콜리전 프로파일 설정

        // 컴포넌트가 어딘가에 부딪힐 때 호출되는 이벤트
        CollisionComponent->OnComponentHit.AddDynamic(this, &AFPSProjectile::OnHit);

        // 루트 컴포넌트가 콜리전 컴포넌트가 되도록 설정
        RootComponent = CollisionComponent;
    }

    if (!ProjectileMovementComponent)
    {
        // 이 컴포넌트를 사용하여 이 발사체의 이동 구현.
        ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
        ProjectileMovementComponent->SetUpdatedComponent(CollisionComponent);
        ProjectileMovementComponent->InitialSpeed = 3000.0f;                    // 초기속도
        ProjectileMovementComponent->MaxSpeed = 3000.0f;                        // 최대 속도
		ProjectileMovementComponent->bRotationFollowsVelocity = true;           // 속도에 따라 회전
		ProjectileMovementComponent->bShouldBounce = true;                      // 바운스 활성화
        ProjectileMovementComponent->Bounciness = 0.3f;
		ProjectileMovementComponent->ProjectileGravityScale = 0.0f;             // 중력의 영향을 받지 않음
    }

    if (!ProjectileMeshComponent)
    {
        ProjectileMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMeshComponent"));
        static ConstructorHelpers::FObjectFinder<UStaticMesh>Mesh(TEXT("/Script/Engine.StaticMesh'/Game/Projectiles/bullet.bullet'"));
        if (Mesh.Succeeded())
        {
            ProjectileMeshComponent->SetStaticMesh(Mesh.Object);
        }
        //동적으로 메타리얼 적용
        static ConstructorHelpers::FObjectFinder<UMaterial>Material(TEXT("/Script/Engine.Material'/Game/Projectiles/M_AK47.M_AK47'"));
        if (Material.Succeeded())
        {
            ProjectileMaterialInstance = UMaterialInstanceDynamic::Create(Material.Object, ProjectileMeshComponent);
        }
        ProjectileMeshComponent->SetMaterial(0, ProjectileMaterialInstance);
        ProjectileMeshComponent->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.9f));
        ProjectileMeshComponent->SetupAttachment(RootComponent);
    }
	// 피격 이펙트 로드
    if(!StoneImpactEffect)
    {
        static ConstructorHelpers::FObjectFinder<UParticleSystem>ImpactEffect(TEXT("/Script/Engine.ParticleSystem'/Game/MilitaryWeapSilver/FX/P_Impact_Stone_Large_01.P_Impact_Stone_Large_01'"));
        if (ImpactEffect.Succeeded())
        {
            StoneImpactEffect = ImpactEffect.Object;
        }
	}
    
    
	InitialLifeSpan = 3.0f; //3초 후에 파괴되도록 생명주기 정해주는 거..?

 
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
        //발사체의 속도가 ProjectileMovementComponent 에 의해 정의되기 때문에 발사 방향만 제공하면 됨
        ProjectileMovementComponent->Velocity = ShootDirection * ProjectileMovementComponent->InitialSpeed;
	}
}

void AFPSProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    // 자기 자신이나 발사자(Owner)는 제외
    AActor* MyOwner = GetOwner();
    if (OtherActor && OtherActor != this && OtherActor != MyOwner)
    {
        // 데미지 적용 (20. f는 데미지 양, 조정 가능)
        UGameplayStatics::ApplyDamage(
            OtherActor,                          // 데미지 받을 액터
            20.f,                                // 데미지 양
            MyOwner ? MyOwner->GetInstigatorController() : nullptr,  // 컨트롤러
            this,                                // 데미지 원인 액터(총알)
            nullptr                              // 데미지 타입
        );

        UE_LOG(LogTemp, Warning, TEXT("ammo damage to %s! "), *GetNameSafe(OtherActor));
    }
	if (OtherActor != this && OtherComponent->IsSimulatingPhysics())  // 스스로와 충돌하는 게 아니고 , 충돌한 컴포넌트가 물리 시뮬레이션을 하고 있다면
    {
		OtherComponent->AddImpulseAtLocation(ProjectileMovementComponent->Velocity * 100.0f, Hit.ImpactPoint);  // 충돌 지점에 발사체의 속도에 비례하는 임펄스를 가함
    }

    if(StoneImpactEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), StoneImpactEffect, GetActorLocation());  // 피격 이펙트 재생
    }
	
    Destroy();  // 충돌처리가 되면 총알을 파괴  
}

