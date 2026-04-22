// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/HealthComponent.h"

// Sets default values for this component's properties
UHealthComponent::UHealthComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
	Health = MaxHealth;							 // 체력을 최대 체력으로 생성 될 때 초기화
}


// Called when the game starts
void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
	GetOwner()->OnTakePointDamage.AddDynamic(this, &UHealthComponent::PointDamageTaken);
}


void UHealthComponent::PointDamageTaken(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	if (Damage <= 0.f) return;
	Health -= Damage;


	FHitResult DummyHit;
	DummyHit.ImpactPoint = HitLocation;
	DummyHit.BoneName = BoneName;
	OnDamaged.Broadcast(Health, Damage, DummyHit);
}

// HitResult 넘겨주지 않는 데미지 함수.
void UHealthComponent::ApplyDamage(float Damage)
{
	if (Damage <= 0.f) return;

	Health -= Damage;
	Health = FMath::Max(Health, 0.f);  	// 체력 음수 방지

	UE_LOG(LogTemp, Warning, TEXT("ApplyDamage: %f, Remaining Health: %f"), Damage, Health);

	OnHealthChanged.Broadcast(Health, Damage);
}

// Called every frame
void UHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

