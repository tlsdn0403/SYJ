#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseZombie.generated.h"

class UHealthComponent;
class UAnimMontage;
class UParticleSystem;
class UNiagaraSystem;
class AActor;

/** 좀비 상태를 나타내는 열거형 */
UENUM(BlueprintType)
enum class EZombieMovementState : uint8
{
	Normal      UMETA(DisplayName = "Normal"),
	Crawling    UMETA(DisplayName = "Crawling"),
	Dead        UMETA(DisplayName = "Dead")
};

UCLASS()
class FPSPROJECT_API ABaseZombie : public ACharacter
{
	GENERATED_BODY()

public:
	ABaseZombie();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void SetNetworkObjectId(uint64 InNetworkObjectId) { NetworkObjectId = InNetworkObjectId; }
	uint64 GetNetworkObjectId() const { return NetworkObjectId; }

	// --- 인터페이스 섹션 (Public) ---
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Zombie")
	bool IsAlive() const { return bIsAlive; }

	UFUNCTION(BlueprintCallable, Category = "Zombie")
	EZombieMovementState GetMovementState() const { return MovementState; }

	UFUNCTION(BlueprintCallable, Category = "Zombie")
	bool IsCrawling() const { return MovementState == EZombieMovementState::Crawling; }

	UFUNCTION(BlueprintCallable, Category = "Zombie")
	bool IsAttacking() const { return bIsAttacking; }

	void Attack();
	void Attack(AActor* TargetActor);
	bool IsTargetInAttackRange(AActor* TargetActor) const;
	void ApplyDirectPursuitInput(const FVector& TargetLocation);

	UFUNCTION(BlueprintCallable, Category = "Zombie")
	void Die();

protected:
	virtual void BeginPlay() override;

	/** 좀비 대미지 처리 내부 로직 */
	UFUNCTION()
	void OnZombieDamaged(float NewHealth, float Damage, const FHitResult& Hit);

	// --- 상속 클래스에서 접근 가능한 컴포넌트 및 에셋 ---
	UPROPERTY(VisibleDefaultsOnly, Category = "Zombie|Mesh")
	USkeletalMeshComponent* ZombieMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie|Components")
	UHealthComponent* HealthComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Effects")
	UParticleSystem* BloodImpactEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Effects")
	TObjectPtr<UNiagaraSystem> HitEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack")
	UAnimMontage* AttackMontage;

	uint64 NetworkObjectId = 0;

private:


	// 8바이트 영역 포인터 및 컨테이너
	TMap<FName, float> BoneDurability;
	TSet<FName> BrokenBones;

	//4바이트 float, int32
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack", meta = (AllowPrivateAccess = "true"))
	float AttackDamage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack", meta = (AllowPrivateAccess = "true"))
	float AttackRange = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Crawling", meta = (AllowPrivateAccess = "true"))
	float CrawlingMaxSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Crawling", meta = (AllowPrivateAccess = "true"))
	float CrawlingCapsuleHalfHeight = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Crawling", meta = (AllowPrivateAccess = "true"))
	float CrawlingCapsuleRadius = 30.0f;


	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Zombie", meta = (AllowPrivateAccess = "true"))
	EZombieMovementState MovementState;


	// 1바이트
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Zombie", meta = (AllowPrivateAccess = "true"))
	bool bIsAlive = true;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Zombie", meta = (AllowPrivateAccess = "true"))
	bool bIsAttacking = false ;



	// --- 내부 헬퍼 함수 (클래스 내부에서만 사용되는 함수) ---
	FVector GetAttackPointForTarget(AActor* TargetActor) const;
	void ApplyAttackDamage(AActor* TargetActor);
	void ApplyAnimationDesync();
	void InitializeBoneDurability();
	FName GetParentBoneForDamage(FName HitBoneName);
	void ProcessBoneDamage(FName BoneName, float Damage, FVector ImpactPoint, FVector ImpactDirection);
	void DismemberLimb(FName BoneName, FVector Impulse, FVector HitLocation);
	void StartCrawling();
	bool IsLegBone(FName BoneName) const;

	UPROPERTY()
	AActor* CurrentAttackTarget = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Animation", meta = (AllowPrivateAccess = "true"))
	float MinAnimationRateScale = 0.92f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Animation", meta = (AllowPrivateAccess = "true"))
	float MaxAnimationRateScale = 1.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Animation", meta = (AllowPrivateAccess = "true"))
	float MaxAnimationStartOffset = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Animation", meta = (AllowPrivateAccess = "true"))
	float AttackMontagePlayRateVariance = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (AllowPrivateAccess = "true"))
	float TurnRateYaw = 540.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (AllowPrivateAccess = "true"))
	float MaxAcceleration = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (AllowPrivateAccess = "true"))
	float BrakingDecelerationWalking = 700.0f;

	float AnimationRateScale = 1.0f;

	UFUNCTION()
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
