#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_ChaseHuman.generated.h"

class AAIController;
class ABaseZombie;

UCLASS()
class FPSPROJECT_API UBTTask_ChaseHuman : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_ChaseHuman();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = "true"))
	float StopDistance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = "true", ClampMin = "0.05"))
	float RepathInterval = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float RepathDistanceThreshold = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = "true"))
	FBlackboardKeySelector TargetPlayerKey;

	float LastMoveRequestTime = -100000.0f;
	FVector LastIssuedMoveGoalLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> LastIssuedTargetActor;
	bool bLastMoveUsedActorGoal = false;

	void ResetMoveRequestState();
	void RequestChaseMove(AAIController* AIController, ABaseZombie* ZombieCharacter, AActor* TargetActor, const FVector& MoveGoalLocation, bool bHasProjectedMoveGoal, bool bForceRequest);
};
