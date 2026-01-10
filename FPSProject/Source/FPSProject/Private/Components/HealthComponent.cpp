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
	GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::DamageTaken); //이 컴포넌트를 장착하고 있는 엑터가 데미지를 받았을 때 DamageTaken 함수를 호출하도록 설정
}


void UHealthComponent::DamageTaken(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* Instigater, AActor* DamageCauser)
{
	if (Damage <= 0.f) return;									 // 데미지가 0 이하일 때는 처리하지 않음

	Health -= Damage;											 // 체력 감소

	  OnDamaged.Broadcast(Health, Damage);

	UE_LOG(LogTemp, Warning, TEXT("[HealthComponent] %s Health: %.1f"), *GetNameSafe(GetOwner()), Health);
	if (Health <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("[HealthComponent] %s die! "), *GetNameSafe(GetOwner()));
		// 필요시 사망 처리:  GetOwner()->Destroy(); 등
	}
}

// Called every frame
void UHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

