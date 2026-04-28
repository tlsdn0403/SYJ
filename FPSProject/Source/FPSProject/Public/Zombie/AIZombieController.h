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
	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float SightRadius = 2200.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float LoseSightRadius = 3200.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float SightHalfAngleDegrees = 85.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float SightAutoSuccessRange = 600.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float TargetMemoryDuration = 1.5f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float TruckTargetMemoryDuration = 8.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float HearingRange = 6500.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float HearingMemoryDuration = 4.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Awareness")
	float TruckAwarenessDistance = 6000.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Awareness")
	float TruckAwarenessHeightTolerance = 2400.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Awareness")
	float PlayerAwarenessDistance = 1200.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Awareness")
	float PlayerAwarenessHeightTolerance = 600.0f;

	float LastTargetSeenTime = -100000.0f;
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	bool bHasKnownTarget = false;
	TWeakObjectPtr<AActor> CurrentTargetActor;

	void RefreshPerceptionConfig();
	AActor* ResolvePrimaryTargetActor() const;
	bool IsZombieAlive() const;
	bool HasActivePerceptionFor(AActor* TargetActor) const;
	bool CanForceAwarenessFor(AActor* TargetActor) const;
	float GetMemoryDurationForTarget(AActor* TargetActor) const;
	void RememberTarget(AActor* TargetActor, const FVector& KnownLocation);
	void ClearCurrentTarget(UBlackboardComponent* BlackboardComponent);
	void UpdateBlackboardTarget(UBlackboardComponent* BlackboardComponent, AActor* TargetActor, const FVector& TargetLocation);

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* UpdatedActor, FAIStimulus Stimulus);
};
