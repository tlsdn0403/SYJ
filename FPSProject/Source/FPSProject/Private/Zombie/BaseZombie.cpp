// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/BaseZombie.h"
#include "Components/HealthComponent.h"
#include "Components/CapsuleComponent.h"

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


