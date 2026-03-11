// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_ChaseHuman.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API UBTTask_ChaseHuman : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
	
public:
	UBTTask_ChaseHuman();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
private:
	// 공격 범위 (이 거리 이하면 공격으로 전환)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = "true"))
	float StopDistance = 300.0f;
	// Blackboard 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = "true"))
	FBlackboardKeySelector TargetPlayerKey;
};
