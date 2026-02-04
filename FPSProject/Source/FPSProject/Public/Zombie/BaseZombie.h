// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseZombie.generated.h"

class UHealthComponent;

UCLASS()
class FPSPROJECT_API ABaseZombie : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseZombie();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;




    // 살아있는지 확인
    UFUNCTION(BlueprintCallable, Category = "Zombie")
    bool IsAlive() const { return bIsAlive; }

    //좀비 매시
    UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
    USkeletalMeshComponent* ZombieMesh;
protected:
    // 체력 컴포넌트 (기존 HealthComponent 재사용)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UHealthComponent* HealthComponent;

    UFUNCTION()
    void OnZombieDamaged(float NewHealth, float Damage, const FHitResult& Hit); // 좀비가 총알에 데미지 입음

    //총알이 좀비랑 피격시 이펙트
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
    UParticleSystem* BloodImpactEffect;

    // 사망 여부
    UPROPERTY(BlueprintReadOnly, Category = "Zombie")
    bool bIsAlive = true;

    //--------------------- 좀비 신체 분해 시스템 ---------------------

    // 부위별 내구도 BoneName, HP
    TMap<FName, float> BoneDurability;

    // 이미 잘린 부위 목록 중복 분해를 막기 위해서
    TSet<FName> BrokenBones;

    // 초기 내구도 설정
    void InitializeBoneDurability();

    // 손가락 같은 곳 맞았을 때 상위 손,팔 같이 상위뼈로 변환해주는 함수
    FName GetParentBoneForDamage(FName HitBoneName);

    // 뼈 내구도 깎기 및 파괴 체크
    void ProcessBoneDamage(FName BoneName, float Damage, FVector ImpactPoint, FVector ImpactDirection);

    // 실제 분해 실행 
    void DismemberLimb(FName BoneName, FVector Impulse, FVector HitLocation);

    // 사망 처리
    UFUNCTION(BlueprintCallable, Category = "Zombie")
    void Die();


};
