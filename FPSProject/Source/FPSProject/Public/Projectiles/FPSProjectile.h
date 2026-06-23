// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/ObjectPoolSubSystem.h"  // pooling
#include "FPSProjectile.generated.h"


class USphereComponent;
class UProjectileMovementComponent;
class UParticleSystem;
class USoundBase;

UCLASS()
class FPSPROJECT_API AFPSProjectile : public AActor , public IFPSPoolableInterface  //풀링 인터페이스 상속 추가
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFPSProjectile();

	// FPSPoolableInterface 구현
	virtual void OnPoolActivate_Implementation() override;
	virtual void OnPoolDeactivate_Implementation() override;
	virtual void OnPoolSpawn_Implementation(const FVector& Location, const FRotator& Rotation) override;

	// 풀로 반환 (Destroy 대신 사용)
	UFUNCTION(BlueprintCallable, Category = "Pool")
	void ReturnToPool();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 자동 반환 타이머
	FTimerHandle LifetimeTimerHandle;

	// 총알 수명 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Pool")
	float LifetimeSeconds = 3.0f;
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// 스피어 콜리전 컴포넌트
	UPROPERTY(VisibleDefaultsOnly, Category = Projectile)
	USphereComponent* CollisionComponent;

	// 발사체 이동 컴포넌트
	UPROPERTY(VisibleAnywhere, Category = Movement)
	UProjectileMovementComponent* ProjectileMovementComponent;


	// 발사체 메시
	UPROPERTY(VisibleDefaultsOnly, Category = Projectile)
	UStaticMeshComponent* ProjectileMeshComponent;

	// 발사체 머티리얼
	UPROPERTY(VisibleDefaultsOnly, Category = Movement)
	UMaterialInstanceDynamic* ProjectileMaterialInstance;


	// 총알이 돌이랑 피격시 이펙트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UParticleSystem* StoneImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Audio")
	USoundBase* ZombieHeadHitSound;




	// 발사 방향으로의 발사체 속도를 초기화
	void FireInDirection(const FVector& ShootDirection);

	// 발사체가 충돌이 일어날 때 호출되는 함수
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);


};
