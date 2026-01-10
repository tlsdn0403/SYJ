// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/BaseZombie.h"
#include "Components/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"

ABaseZombie::ABaseZombie()
{
    PrimaryActorTick.bCanEverTick = true;

    // 좀비 메시 컴포넌트를 생성.
    ZombieMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ZombieMeshMesh"));
    check(ZombieMesh != nullptr);

    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
}

void ABaseZombie::BeginPlay()
{
    Super::BeginPlay();

    if (HealthComponent)
    {
		HealthComponent->OnDamaged.AddDynamic(this, &ABaseZombie::OnZombieDamaged); // 데미지 입을 때 OnZombieDamaged를 호출
    }
}

void ABaseZombie::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ABaseZombie::Die()
{
    if (!bIsAlive) return;

    bIsAlive = false;

    UE_LOG(LogTemp, Warning, TEXT("Zombie %s Died!"), *GetName());

    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetSimulatePhysics(true);

    SetLifeSpan(5.f);
}

void ABaseZombie::OnZombieDamaged(float NewHealth, float Damage)
{
    // 피 이펙트 재생 (충돌 위치가 넘어오면 Hit.ImpactPoint 사용, 없으면 GetActorLocation 에서 이펙트 재생)
    FVector EffectLocation = GetActorLocation();


    if (BloodImpactEffect)
    {
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodImpactEffect, EffectLocation);
    }

    if (NewHealth <= 0.f && bIsAlive)
    {
        Die();
    }
}


