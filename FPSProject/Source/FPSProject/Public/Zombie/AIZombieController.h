// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AIZombieController.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API AAIZombieController : public AAIController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnyWhere)
	class UBehaviorTree* ZombieBehaviorTree;

	// 블랙보드 키 이름
	static const FName TargetPlayerKey;

public:
	APawn* PlayerPawn;

private:
	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float SightDotThreshold = 0.7f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float TargetMemoryDuration = 1.5f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float TruckTargetMemoryDuration = 8.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Perception")
	float TruckAwarenessDistance = 6000.0f;

	float LastTargetSeenTime = -100000.0f;
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	bool bHasKnownTarget = false;

};
