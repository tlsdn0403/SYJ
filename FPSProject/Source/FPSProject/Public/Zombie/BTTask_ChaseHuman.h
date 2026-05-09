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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fallback", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DirectPursuitRange2D = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fallback", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DirectPursuitMaxRise = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fallback", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DirectPursuitMaxDrop = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fallback", meta = (AllowPrivateAccess = "true"))
	bool bRequireLineOfSightForDirectPursuit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = "true"))
	FBlackboardKeySelector TargetPlayerKey;

	float LastMoveRequestTime = -100000.0f;
	TWeakObjectPtr<AActor> LastIssuedTargetActor;

	void ResetMoveRequestState();
	bool RequestChaseMove(AAIController* AIController, ABaseZombie* ZombieCharacter, AActor* TargetActor, bool bForceRequest);
	bool ShouldUseDirectPursuitFallback(AAIController* AIController, ABaseZombie* ZombieCharacter, AActor* TargetActor) const;
	void ApplyDirectPursuitFallback(AAIController* AIController, ABaseZombie* ZombieCharacter, AActor* TargetActor) const;
};
