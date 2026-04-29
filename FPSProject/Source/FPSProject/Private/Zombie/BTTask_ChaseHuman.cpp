#include "Zombie/BTTask_ChaseHuman.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "NavigationSystem.h"
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

	FVector GetClosestPointOnTarget(AActor* TargetActor, const FVector& FromLocation)
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

	bool TryGetMovePointOnNavigation(AActor* TargetActor, const FVector& FromLocation, FVector& OutMoveGoalLocation)
	{
		const FVector TargetReachPoint = GetClosestPointOnTarget(TargetActor, FromLocation);
		if (!TargetActor)
		{
			OutMoveGoalLocation = TargetReachPoint;
			return false;
		}

		if (UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(TargetActor))
		{
			FNavLocation ProjectedLocation;
			const FVector QueryExtent(300.0f, 300.0f, 300.0f);
			if (NavSystem->ProjectPointToNavigation(TargetReachPoint, ProjectedLocation, QueryExtent))
			{
				OutMoveGoalLocation = ProjectedLocation.Location;
				return true;
			}
		}

		OutMoveGoalLocation = TargetReachPoint;
		return false;
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
	LastMoveRequestTime = -100000.0f;
	LastIssuedMoveGoalLocation = FVector::ZeroVector;
	LastIssuedTargetActor.Reset();
	bLastMoveUsedActorGoal = false;
}

void UBTTask_ChaseHuman::RequestChaseMove(AAIController* AIController, ABaseZombie* ZombieCharacter, AActor* TargetActor, const FVector& MoveGoalLocation, bool bHasProjectedMoveGoal, bool bForceRequest)
{
	if (!AIController || !ZombieCharacter || !TargetActor)
	{
		return;
	}

	const bool bUseActorGoal = TargetActor->IsA<ATruck>() || !bHasProjectedMoveGoal;
	const float CurrentTime = AIController->GetWorld() ? AIController->GetWorld()->GetTimeSeconds() : 0.0f;
	const bool bTargetChanged = LastIssuedTargetActor.Get() != TargetActor;
	const bool bGoalTypeChanged = bUseActorGoal != bLastMoveUsedActorGoal;
	const bool bMoveStopped = AIController->GetMoveStatus() != EPathFollowingStatus::Moving;
	const bool bEnoughTimePassed = (CurrentTime - LastMoveRequestTime) >= RepathInterval;
	const bool bGoalMovedEnough =
		!bUseActorGoal &&
		FVector::DistSquared2D(LastIssuedMoveGoalLocation, MoveGoalLocation) >= FMath::Square(RepathDistanceThreshold);

	if (!bForceRequest &&
		!bTargetChanged &&
		!bGoalTypeChanged &&
		!bMoveStopped &&
		!bEnoughTimePassed &&
		!bGoalMovedEnough)
	{
		return;
	}

	EPathFollowingRequestResult::Type RequestResult = EPathFollowingRequestResult::Failed;
	if (bUseActorGoal)
	{
		RequestResult = AIController->MoveToActor(TargetActor, StopDistance, true, true, true, nullptr, true);
	}
	else
	{
		RequestResult = AIController->MoveToLocation(MoveGoalLocation, StopDistance, true, true, true, false);
	}

	if (RequestResult != EPathFollowingRequestResult::Failed)
	{
		LastMoveRequestTime = CurrentTime;
		LastIssuedMoveGoalLocation = MoveGoalLocation;
		LastIssuedTargetActor = TargetActor;
		bLastMoveUsedActorGoal = bUseActorGoal;
	}
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

	if (!TargetActor || !AIController || !ZombieCharacter || !ZombieCharacter->IsAlive())
	{
		return EBTNodeResult::Failed;
	}

	AIController->SetFocus(TargetActor);

	FVector MoveGoalLocation = GetClosestPointOnTarget(TargetActor, ZombieCharacter->GetActorLocation());
	const bool bHasProjectedMoveGoal = TryGetMovePointOnNavigation(TargetActor, ZombieCharacter->GetActorLocation(), MoveGoalLocation);
	RequestChaseMove(AIController, ZombieCharacter, TargetActor, MoveGoalLocation, bHasProjectedMoveGoal, true);

	return EBTNodeResult::InProgress;
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

	const FVector TargetReachPoint = GetClosestPointOnTarget(TargetActor, ZombieCharacter->GetActorLocation());
	const float Distance = FVector::Dist(ZombieCharacter->GetActorLocation(), TargetReachPoint);

	if (Distance <= StopDistance)
	{
		AIController->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	FVector MoveGoalLocation = TargetReachPoint;
	const bool bHasProjectedMoveGoal = TryGetMovePointOnNavigation(TargetActor, ZombieCharacter->GetActorLocation(), MoveGoalLocation);
	RequestChaseMove(AIController, ZombieCharacter, TargetActor, MoveGoalLocation, bHasProjectedMoveGoal, false);
}
