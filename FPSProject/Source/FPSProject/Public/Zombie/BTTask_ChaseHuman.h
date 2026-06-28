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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Truck", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MovingTruckSpeedThreshold = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Truck", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float StoppedTruckSpeedThreshold = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Truck", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MovingTruckAcceptanceRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Truck", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TruckApproachAcceptanceRadius = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Truck", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TruckApproachRetargetDistance = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fall Zone", meta = (AllowPrivateAccess = "true"))
	bool bUseFallZone = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fall Zone", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FallZoneApproachAcceptanceRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fall Zone", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FallZoneCommitDistance = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fall Zone", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FallZoneRetargetDistance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (AllowPrivateAccess = "true"))
	FBlackboardKeySelector TargetPlayerKey;

	TWeakObjectPtr<AActor> LastIssuedTargetActor;
	FVector LastIssuedFallZoneLocation = FVector::ZeroVector;
	FVector LastIssuedTruckApproachLocation = FVector::ZeroVector;
	bool bHasLastIssuedFallZoneLocation = false;
	bool bHasLastIssuedTruckApproachLocation = false;
	bool bHasLastIssuedMovingTruckSetting = false;
	bool bLastIssuedMovingTruckSetting = false;
	TWeakObjectPtr<AActor> LastEvaluatedTruckTarget;
	bool bHasTruckChaseMode = false;
	bool bTruckChaseMode = false;

	void ResetMoveRequestState();
	bool RequestChaseMove(AAIController* AIController, ABaseZombie* ZombieCharacter, AActor* TargetActor, bool bForceRequest);
	bool RequestFallZoneMove(AAIController* AIController, const FVector& FallZoneLocation, bool bForceRequest);
	bool TryUseFallZone(AAIController* AIController, ABaseZombie* ZombieCharacter, AActor* TargetActor);
	bool IsTargetInStopDistance(ABaseZombie* ZombieCharacter, AActor* TargetActor);
	bool ShouldUseMovingTruckChase(AActor* TargetActor);
	float GetChaseAcceptanceRadius(AActor* TargetActor) const;
};