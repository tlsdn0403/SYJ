#include "Zombie/BTTask_ChaseHuman.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Truck/Truck.h"
#include "Zombie/BaseZombie.h"

namespace
{
	AActor* ResolveChaseTarget(AActor* BlackboardTarget)
	{
		AFPSBaseCharacter* PlayerCharacter = Cast<AFPSBaseCharacter>(BlackboardTarget);
		if (PlayerCharacter &&
			IsValid(PlayerCharacter->CurrentTruck) &&
			(PlayerCharacter->IsDrivingTruck() ||
				PlayerCharacter->IsOnTruckCargo() ||
				PlayerCharacter->IsUsingMountedWeapon()))
		{
			return PlayerCharacter->CurrentTruck;
		}

		return BlackboardTarget;
	}

	FVector GetClosestPointOnSimpleChaseTarget(AActor* TargetActor, const FVector& FromLocation)
	{
		if (!TargetActor)
		{
			return FromLocation;
		}

		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(TargetActor->GetRootComponent()))
		{
			FVector ClosestPoint = TargetActor->GetActorLocation();
			if (PrimitiveComponent->GetClosestPointOnCollision(FromLocation, ClosestPoint) >= 0.0f)
			{
				return ClosestPoint;
			}
		}

		return TargetActor->GetActorLocation();
	}
}

UBTTask_ChaseHuman::UBTTask_ChaseHuman()
{
	NodeName = TEXT("Chase Human");
	TargetPlayerKey.SelectedKeyName = FName("TargetPlayer");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

void UBTTask_ChaseHuman::ResetMoveRequestState()
{
	LastIssuedTargetActor.Reset();
	LastIssuedFallZoneLocation = FVector::ZeroVector;
	bHasLastIssuedFallZoneLocation = false;
	bHasLastIssuedMovingTruckSetting = false;
	bLastIssuedMovingTruckSetting = false;
	LastEvaluatedTruckTarget.Reset();
	bHasTruckChaseMode = false;
	bTruckChaseMode = false;
}

bool UBTTask_ChaseHuman::RequestChaseMove(AAIController* AIController, AActor* TargetActor, bool bForceRequest)
{
	if (!AIController || !TargetActor)
	{
		return false;
	}

	UPathFollowingComponent* PathFollowingComponent = AIController->GetPathFollowingComponent();
	const EPathFollowingStatus::Type MoveStatus = PathFollowingComponent
		? PathFollowingComponent->GetStatus()
		: AIController->GetMoveStatus();
	const bool bTargetChanged = LastIssuedTargetActor.Get() != TargetActor;
	const bool bMovingTruckTarget = ShouldUseMovingTruckChase(TargetActor);
	const bool bMoveSettingsChanged =
		!bHasLastIssuedMovingTruckSetting ||
		bLastIssuedMovingTruckSetting != bMovingTruckTarget;

	if (!bForceRequest && !bTargetChanged && !bMoveSettingsChanged && MoveStatus != EPathFollowingStatus::Idle)
	{
		return true;
	}

	const float AcceptanceRadius = GetChaseAcceptanceRadius(TargetActor);
	const EPathFollowingRequestResult::Type RequestResult =
		AIController->MoveToActor(TargetActor, AcceptanceRadius, !bMovingTruckTarget, true, true, nullptr, true);

	if (RequestResult != EPathFollowingRequestResult::Failed)
	{
		LastIssuedTargetActor = TargetActor;
		bHasLastIssuedMovingTruckSetting = true;
		bLastIssuedMovingTruckSetting = bMovingTruckTarget;
		return true;
	}

	return false;
}

bool UBTTask_ChaseHuman::RequestFallZoneMove(AAIController* AIController, const FVector& FallZoneLocation, bool bForceRequest)
{
	if (!AIController)
	{
		return false;
	}

	UPathFollowingComponent* PathFollowingComponent = AIController->GetPathFollowingComponent();
	const EPathFollowingStatus::Type MoveStatus = PathFollowingComponent
		? PathFollowingComponent->GetStatus()
		: AIController->GetMoveStatus();
	const bool bUsingCustomLink = PathFollowingComponent && PathFollowingComponent->GetCurrentCustomLinkOb() != nullptr;
	const bool bLocationChanged =
		!bHasLastIssuedFallZoneLocation ||
		FVector::DistSquared2D(LastIssuedFallZoneLocation, FallZoneLocation) >= FMath::Square(FallZoneRetargetDistance);

	if (!bForceRequest && (bUsingCustomLink || (!bLocationChanged && MoveStatus != EPathFollowingStatus::Idle)))
	{
		return true;
	}

	const EPathFollowingRequestResult::Type RequestResult =
		AIController->MoveToLocation(FallZoneLocation, FallZoneApproachAcceptanceRadius, true, true, true, false, nullptr, true);

	if (RequestResult != EPathFollowingRequestResult::Failed)
	{
		LastIssuedTargetActor.Reset();
		LastIssuedFallZoneLocation = FallZoneLocation;
		bHasLastIssuedFallZoneLocation = true;
		return true;
	}

	return false;
}

bool UBTTask_ChaseHuman::TryUseFallZone(AAIController* AIController, ABaseZombie* ZombieCharacter, AActor* TargetActor)
{
	if (!bUseFallZone || !AIController || !ZombieCharacter || !TargetActor)
	{
		return false;
	}

	FVector ApproachLocation = FVector::ZeroVector;
	FVector CommitLocation = FVector::ZeroVector;
	if (!ZombieCharacter->TryGetFallZonePursuitLocation(TargetActor, ApproachLocation, CommitLocation))
	{
		return false;
	}

	AIController->SetFocus(TargetActor);

	const float DistanceToApproach = FVector::Dist2D(ZombieCharacter->GetActorLocation(), ApproachLocation);
	if (DistanceToApproach > FallZoneCommitDistance)
	{
		return RequestFallZoneMove(AIController, ApproachLocation, false);
	}

	AIController->StopMovement();
	ZombieCharacter->ApplyDirectPursuitInput(CommitLocation);
	return true;
}

bool UBTTask_ChaseHuman::IsTargetInStopDistance(ABaseZombie* ZombieCharacter, AActor* TargetActor)
{
	if (!ZombieCharacter || !TargetActor)
	{
		return false;
	}

	if (ShouldUseMovingTruckChase(TargetActor))
	{
		return false;
	}

	const FVector TargetPoint = GetClosestPointOnSimpleChaseTarget(TargetActor, ZombieCharacter->GetActorLocation());
	return FVector::Dist(ZombieCharacter->GetActorLocation(), TargetPoint) <= StopDistance;
}

bool UBTTask_ChaseHuman::ShouldUseMovingTruckChase(AActor* TargetActor)
{
	const ATruck* Truck = Cast<ATruck>(TargetActor);
	if (!Truck || MovingTruckSpeedThreshold <= 0.0f)
	{
		LastEvaluatedTruckTarget.Reset();
		bHasTruckChaseMode = false;
		bTruckChaseMode = false;
		return false;
	}

	if (LastEvaluatedTruckTarget.Get() != TargetActor)
	{
		LastEvaluatedTruckTarget = TargetActor;
		bHasTruckChaseMode = false;
		bTruckChaseMode = false;
	}

	const float StartMovingThreshold = MovingTruckSpeedThreshold;
	const float StopMovingThreshold = FMath::Clamp(StoppedTruckSpeedThreshold, 0.0f, StartMovingThreshold);
	const float SpeedSquared2D = Truck->GetVelocity().SizeSquared2D();

	if (!bHasTruckChaseMode)
	{
		bTruckChaseMode = SpeedSquared2D >= FMath::Square(StartMovingThreshold);
		bHasTruckChaseMode = true;
		return bTruckChaseMode;
	}

	if (bTruckChaseMode)
	{
		bTruckChaseMode = SpeedSquared2D > FMath::Square(StopMovingThreshold);
	}
	else
	{
		bTruckChaseMode = SpeedSquared2D >= FMath::Square(StartMovingThreshold);
	}

	return bTruckChaseMode;
}

float UBTTask_ChaseHuman::GetChaseAcceptanceRadius(AActor* TargetActor) const
{
	return Cast<ATruck>(TargetActor)
		? FMath::Min(StopDistance, MovingTruckAcceptanceRadius)
		: StopDistance;
}

EBTNodeResult::Type UBTTask_ChaseHuman::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ResetMoveRequestState();

	AAIController* AIController = OwnerComp.GetAIOwner();
	ABaseZombie* ZombieCharacter = AIController ? Cast<ABaseZombie>(AIController->GetPawn()) : nullptr;
	UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	AActor* BlackboardTarget = BlackboardComponent
		? Cast<AActor>(BlackboardComponent->GetValueAsObject(TargetPlayerKey.SelectedKeyName))
		: nullptr;
	AActor* TargetActor = ResolveChaseTarget(BlackboardTarget);

	if (!AIController || !ZombieCharacter || !TargetActor || !ZombieCharacter->IsAlive())
	{
		return EBTNodeResult::Failed;
	}

	AIController->SetFocus(TargetActor);
	if (TryUseFallZone(AIController, ZombieCharacter, TargetActor))
	{
		return EBTNodeResult::InProgress;
	}

	return RequestChaseMove(AIController, TargetActor, true)
		? EBTNodeResult::InProgress
		: EBTNodeResult::Failed;
}

void UBTTask_ChaseHuman::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	ABaseZombie* ZombieCharacter = Cast<ABaseZombie>(AIController->GetPawn());
	UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent();
	AActor* BlackboardTarget = BlackboardComponent
		? Cast<AActor>(BlackboardComponent->GetValueAsObject(TargetPlayerKey.SelectedKeyName))
		: nullptr;
	AActor* TargetActor = ResolveChaseTarget(BlackboardTarget);

	if (!ZombieCharacter || !TargetActor || !ZombieCharacter->IsAlive())
	{
		AIController->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AIController->SetFocus(TargetActor);

	if (TryUseFallZone(AIController, ZombieCharacter, TargetActor))
	{
		return;
	}

	if (IsTargetInStopDistance(ZombieCharacter, TargetActor))
	{
		AIController->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	if (!RequestChaseMove(AIController, TargetActor, false))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
	}
}
