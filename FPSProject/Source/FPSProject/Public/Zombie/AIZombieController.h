// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "AIZombieController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UBehaviorTree;
class UBlackboardComponent;
class USoundBase;

UCLASS()
class FPSPROJECT_API AAIZombieController : public AAIController
{
	GENERATED_BODY()

public:
	AAIZombieController();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Behavior")
	TObjectPtr<UBehaviorTree> ZombieBehaviorTree = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAIPerceptionComponent> ZombiePerceptionComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig = nullptr;

	static const FName TargetPlayerKey;
	static const FName PlayerLocationKey;

public:
	UPROPERTY(Transient)
	TObjectPtr<APawn> PlayerPawn = nullptr;

private:
	UPROPERTY(EditAnywhere, Category = "AI|Performance", meta = (ClampMin = "0.01"))
	float ControllerUpdateInterval = 0.10f;

	UPROPERTY(EditAnywhere, Category = "AI|Performance", meta = (ClampMin = "0.05"))
	float PlayerPawnRefreshInterval = 0.50f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float SightRadius = 4500.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float LoseSightRadius = 7000.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float SightHalfAngleDegrees = 85.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float SightAutoSuccessRange = 600.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float TargetMemoryDuration = 1.5f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float TruckTargetMemoryDuration = 10.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float HearingRange = 12000.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float HearingMemoryDuration = 4.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Awareness")
	float TruckAwarenessDistance = 12000.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Awareness")
	float TruckAwarenessHeightTolerance = 10000.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Awareness")
	float PlayerAwarenessDistance = 1200.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Awareness")
	float PlayerAwarenessHeightTolerance = 600.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Awareness|Sound")
	TObjectPtr<USoundBase> ZombieGroupAwarenessSound = nullptr;

	UPROPERTY(EditAnywhere, Category = "AI|Awareness|Sound", meta = (ClampMin = "1"))
	int32 MaxZombieGroupAwarenessSoundPlays = 1;

	UPROPERTY(EditAnywhere, Category = "AI|Awareness|Sound", meta = (ClampMin = "0.0"))
	float ZombieGroupAwarenessSoundCooldown = 2.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Awareness|Sound", meta = (ClampMin = "100.0"))
	float ZombieGroupFallbackCellSize = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Navigation")
	bool bRequireReachableNavigationPath = true;

	UPROPERTY(EditAnywhere, Category = "AI|Navigation", meta = (ClampMin = "0.05"))
	float NavigationPathCheckInterval = 0.35f;

	UPROPERTY(EditAnywhere, Category = "AI|Blackboard", meta = (ClampMin = "0.0"))
	float BlackboardLocationUpdateDistance = 100.0f;

	float LastTargetSeenTime = -100000.0f;
	float LastPlayerPawnRefreshTime = -100000.0f;
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	bool bHasKnownTarget = false;
	TWeakObjectPtr<AActor> CurrentTargetActor;
	TWeakObjectPtr<AActor> CachedReachabilityTargetActor;
	float LastReachabilityCheckTime = -100000.0f;
	bool bCachedReachabilityResult = false;

	void RefreshPerceptionConfig();
	AActor* ResolvePrimaryTargetActor() const;
	bool IsZombieAlive() const;
	bool HasActivePerceptionFor(AActor* TargetActor) const;
	bool CanForceAwarenessFor(AActor* TargetActor) const;
	bool HasReachableNavigationPathTo(AActor* TargetActor);
	float GetMemoryDurationForTarget(AActor* TargetActor) const;
	int32 ResolveZombieGroupSoundKey() const;
	void TryPlayZombieGroupAwarenessSound(AActor* TargetActor, const FVector& KnownLocation);
	void RememberTarget(AActor* TargetActor, const FVector& KnownLocation);
	void ClearCurrentTarget(UBlackboardComponent* BlackboardComponent);
	void UpdateBlackboardTarget(UBlackboardComponent* BlackboardComponent, AActor* TargetActor, const FVector& TargetLocation);

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* UpdatedActor, FAIStimulus Stimulus);
};
