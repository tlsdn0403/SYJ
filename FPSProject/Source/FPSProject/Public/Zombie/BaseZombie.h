// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseZombie.generated.h"

class UHealthComponent;

// 좀비 상태를 나타내는 enum
UENUM(BlueprintType)
enum class EZombieMovementState : uint8
{
    Normal     UMETA(DisplayName = "Normal"),
    Crawling   UMETA(DisplayName = "Crawling"),
    Dead       UMETA(DisplayName = "Dead")
};


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

    // 현재 이동 상태 (AnimBP에서 사용)
    UFUNCTION(BlueprintCallable, Category = "Zombie")
    EZombieMovementState GetMovementState() const { return MovementState; }

    // 크롤링 중인지 확인 (AnimBP에서 사용)
    UFUNCTION(BlueprintCallable, Category = "Zombie")
    bool IsCrawling() const { return MovementState == EZombieMovementState::Crawling; }

    // 공격 중인지 (AnimBP에서 사용)
    UFUNCTION(BlueprintCallable, Category = "Zombie")
    bool IsAttacking() const { return bIsAttacking; }

    //좀비 매시
    UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
    USkeletalMeshComponent* ZombieMesh;

    // 좀비 공격
    void Attack();


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


    // 이동 상태
    UPROPERTY(BlueprintReadOnly, Category = "Zombie")
    EZombieMovementState MovementState = EZombieMovementState::Normal;


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



    // 크롤링 전환
    void StartCrawling();

    // 하체 뼈인지 확인
    bool IsLegBone(FName BoneName) const;

    //---------------------------------------------------------------------------------------------------------
    // 크롤링 관련 설정 
    //---------------------------------------------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Crawling")
    float CrawlingMaxSpeed = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Crawling")
    float CrawlingCapsuleHalfHeight = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Crawling")
    float CrawlingCapsuleRadius = 30.0f;

    //---------------------------------------------------------------------------------------------------------
    // 좀비 공격 관련
    //---------------------------------------------------------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "Zombie|Attack")
    bool bIsAttacking = false;

    // 공격 데미지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack")
    float AttackDamage = 15.0f;

    // 공격 사거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack")
    float AttackRange = 200.0f;

    // 공격 애니메이션 몽타주 (에디터에서 할당)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack")
    UAnimMontage* AttackMontage;

    // 몽타주 끝났을 때 호출
    UFUNCTION()
    void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
    // 사망 처리
    UFUNCTION(BlueprintCallable, Category = "Zombie")
    void Die();

 
};
