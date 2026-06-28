#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "BaseZombie.generated.h"

class UHealthComponent;
class UAnimMontage;
class UAnimInstance;
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
	float GetTruckAttackStandOffDistance() const;
	void ApplyDirectPursuitInput(const FVector& TargetLocation);
	bool TryGetFallZonePursuitLocation(AActor* TargetActor, FVector& OutApproachLocation, FVector& OutCommitLocation);

	UFUNCTION(BlueprintCallable, Category = "Zombie")
	void Die();

	void HandleNetworkAttack(AActor* TargetActor, bool bShouldApplyDamage);
	void HandleNetworkHit(float NewHealth, float MaxHealth);
	void HandleNetworkDeath();
	void HandleNetworkDismember(FName BoneName, const FVector& Impulse, const FVector& HitLocation);
	void SetNetworkMoveTarget(const FVector& TargetLocation, const FRotator& TargetRotation, bool bInIsMoving);
	void ApplyTruckImpactKnockback(const FVector& LaunchVelocity, const FVector& RagdollImpulse, const FVector& ImpactPoint, bool bForceRagdoll, float NetworkMovePauseSeconds = 0.0f);

protected:
	virtual void BeginPlay() override;
	virtual float GetDirectAttackAnimationDuration();
	virtual float PlayDeathAnimationBeforeRagdoll();
	virtual void OnNetworkMoveAnimationUpdated();
	virtual FVector GetCrawlingMeshRelativeLocation(const FVector& CurrentStandingMeshRelativeLocation) const;
	AActor* GetCurrentAttackTarget() const { return CurrentAttackTarget; }
	float GetNetworkAnimationMoveSpeed2D() const { return NetworkAnimationMoveSpeed2D; }
	FName GetLastDeathHitBone() const { return LastDeathHitBone; }
	bool WasCrawlingBeforeDeath() const { return bWasCrawlingBeforeDeath; }

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Dismember")
	TMap<FName, TSubclassOf<AActor>> DismemberChunkClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Dismember")
	TSubclassOf<AActor> DefaultDismemberChunkClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Dismember", meta = (ClampMin = "0.0"))
	float DismemberChunkLifeSpan = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack")
	UAnimMontage* AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack")
	UAnimMontage* CrawlingAttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Zombie|Animation", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UAnimInstance> ZombieAnimClass;

	uint64 NetworkObjectId = 0;

	virtual void InitializeBoneDurability();
	virtual FName GetParentBoneForDamage(FName HitBoneName) const;
	virtual FName GetPhysicsRootBoneName() const;
	virtual bool IsFatalDismemberBone(FName BoneName) const;
	virtual bool IsLegBone(FName BoneName) const;

	void ResetDismemberBones();
	void RegisterDismemberBone(FName BoneName, float Durability);

private:


	// 8바이트 영역 포인터 및 컨테이너
	TMap<FName, float> BoneDurability;
	TSet<FName> BrokenBones;

	//4바이트 float, int32
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack", meta = (AllowPrivateAccess = "true"))
	float AttackDamage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack", meta = (AllowPrivateAccess = "true"))
	float AttackRange = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TruckAttackStandOffPadding = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TruckAttackPositionTolerance = 45.0f;

	// 몽타주 45퍼 진행되면 데미지를 줌
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "0.95"))
	float AttackDamageTimeRatio = 0.23f;

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
	AActor* ResolveAttackDamageTarget() const;
	void ScheduleAttackDamage(float MontageDuration);
	void TriggerAttackDamage();
	void FinishAttack();
	void CancelAttack();
	void ApplyAttackDamage(AActor* TargetActor);
	void ApplyAnimationDesync();
	void ApplyMovementTuning();
	void ApplyAvoidanceTuning();
	void FaceAttackTarget(AActor* TargetActor);
	UAnimMontage* GetAttackMontageForCurrentState() const;
	void StartAttack(AActor* TargetActor, bool bAllowFallbackTarget);
	void ProcessBoneDamage(FName BoneName, float Damage, FVector ImpactPoint, FVector ImpactDirection);
	void DismemberLimb(FName BoneName, FVector Impulse, FVector HitLocation);
	void SpawnDismemberChunk(FName BoneName, const FVector& Impulse, const FVector& HitLocation);
	void StartCrawling();
	void StartDeathVisual();
	void EnableDeathRagdoll();
	void ResumeNetworkMovementAfterImpact();

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FallbackAttackDuration = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (AllowPrivateAccess = "true"))
	float TurnRateYaw = 540.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (AllowPrivateAccess = "true"))
	float MaxAcceleration = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (AllowPrivateAccess = "true"))
	float BrakingDecelerationWalking = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement|Avoidance", meta = (AllowPrivateAccess = "true"))
	bool bUseRVOAvoidance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement|Avoidance", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AvoidanceConsiderationRadius = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement|Avoidance", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float CrawlingAvoidanceConsiderationRadius = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement|Avoidance", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AvoidanceWeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (AllowPrivateAccess = "true"))
	float NetworkMoveInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (AllowPrivateAccess = "true"))
	float NetworkRotationInterpSpeed = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Movement", meta = (AllowPrivateAccess = "true"))
	float NetworkMoveSnapDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zombie|Animation", meta = (AllowPrivateAccess = "true", ClampMin = "0.1"))
	float NetworkAnimBlueprintVelocityScale = 2.5f;

	float AnimationRateScale = 1.0f;
	FVector StandingMeshRelativeLocation = FVector::ZeroVector;
	bool bHasStandingMeshRelativeLocation = false;
	bool bHasNetworkMoveTarget = false;
	bool bNetworkTargetIsMoving = false;
	float NetworkAnimationMoveSpeed2D = 0.0f;
	FVector NetworkTargetLocation = FVector::ZeroVector;
	FRotator NetworkTargetRotation = FRotator::ZeroRotator;
	FTimerHandle AttackDamageTimerHandle;
	FTimerHandle AttackFinishTimerHandle;
	FTimerHandle DeathRagdollTimerHandle;
	FTimerHandle NetworkImpactRecoveryTimerHandle;
	FName LastDeathHitBone = NAME_None;
	bool bWasCrawlingBeforeDeath = false;
	bool bAttackDamageApplied = false;
	bool bShouldApplyCurrentNetworkAttackDamage = true;
	bool bNetworkImpactTemporarilyEnabledMovement = false;
	float NetworkMovePausedUntilTime = 0.0f;

	UFUNCTION()
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};