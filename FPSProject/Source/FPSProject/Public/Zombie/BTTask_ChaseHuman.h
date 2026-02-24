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

private:
	// Blackboard Ű
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = "true"))
	FBlackboardKeySelector TargetPlayerKey;
};
