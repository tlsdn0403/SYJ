// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

// 좀비 분해를 위해 피격 정보를 알려주는 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDamagedByBullet, float, Health, float, Damage, const FHitResult&, HitResult);
// 그냥 데미지를 넘겨주는 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, NewHealth, float, Damage);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UHealthComponent();

	// 외부에서 직접 데미지를 주는 함수 (좀비 공격용)
	UFUNCTION(BlueprintCallable, Category = "Health")
	void ApplyDamage(float Damage);

	void ApplyDamageSilently(float Damage);
	
	// 체력 Getter
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return Health; }
	UFUNCTION(BlueprintPure, Category = "Health")

	float MaxGetHealth() const { return MaxHealth; }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:

	UPROPERTY(EditAnywhere)
	float MaxHealth = 100.f;
	float Health = 0.f;

	void ApplyDamageInternal(float Damage, bool bBroadcastHealthChanged);

	UFUNCTION()
	void PointDamageTaken(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation,
		UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser);
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintAssignable, Category = "Hit")
	FOnDamagedByBullet OnDamaged;
	

	// 체력 변동 이벤트 (UI 등)
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHealthChanged;
};