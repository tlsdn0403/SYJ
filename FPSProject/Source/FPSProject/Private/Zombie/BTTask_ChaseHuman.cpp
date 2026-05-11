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

	FVector GetClosestPointOnChaseTarget(AActor* TargetActor, const FVector& FromLocation)
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
	LastMoveRequestTime = -100000.0f;
	LastIssuedTargetActor.Reset();
	bUsingFallZonePursuit = false;
}

bool UBTTask_ChaseHuman::RequestChaseMove(AAIController* AIController, ABaseZombie* ZombieCharacter, AActor* TargetActor, bool bForceRequest)
{
	if (!AIController || !ZombieCharacter || !TargetActor)
	{
		return false;
	}

	const float CurrentTime = AIController->GetWorld() ? AIController->GetWorld()->GetTimeSeconds() : 0.0f;
	const bool bTargetChanged = LastIssuedTargetActor.Get() != TargetActor;
	UPathFollowingComponent* PathFollowingComponent = AIController->GetPathFollowingComponent();
	const EPathFollowingStatus::Type MoveStatus = PathFollowingComponent
		? PathFollowingComponent->GetStatus()
		: AIController->GetMoveStatus();
	const bool bUsingCustomLink = PathFollowingComponent && PathFollowingComponent->GetCurrentCustomLinkOb() != nullptr;
	const bool bMoveNeedsRestart = MoveStatus == EPathFollowingStatus::Idle;
	const bool bEnoughTimePassed = (CurrentTime - LastMoveRequestTime) >= RepathInterval;
	const bool bShouldRetryMove = bMoveNeedsRestart && bEnoughTimePassed;

	if (!bForceRequest &&
		(bUsingCustomLink || (!bTargetChanged && !bShouldRetryMove)))
	{
		return true;
	}

	const EPathFollowingRequestResult::Type RequestResult =
		AIController->MoveToActor(TargetActor, StopDistance, true, true, true, nullptr, true);

	if (RequestResult != EPathFollowingRequestResult::Failed)
	{
		LastMoveRequestTime = CurrentTime;
		LastIssuedTargetActor = TargetActor;
		return true;
	}

	return false;
}

bool UBTTask_ChaseHuman::TryUseFallZonePursuit(AAIController* AIController, ABaseZombie* ZombieCharacter, AActor* TargetActor)
{
	if (!AIController || !ZombieCharacter || !TargetActor)
	{
		return false;
	}

	FVector FallZonePursuitLocation = FVector::ZeroVector;
	if (!ZombieCharacter->TryGetFallZonePursuitLocation(TargetActor, FallZonePursuitLocation))
	{
		return false;
	}

	if (!bUsingFallZonePursuit)
	{
		AIController->StopMovement();
		bUsingFallZonePursuit = true;
	}

	AIController->SetFocus(TargetActor);
	ZombieCharacter->ApplyDirectPursuitInput(FallZonePursuitLocation);
	return true;
}

bool UBTTask_ChaseHuman::ShouldUseDirectPursuitFallback(AAIController* AIController, ABaseZombie* ZombieCharacter, AActor* TargetActor) const
{
	if (!AIController || !ZombieCharacter || !TargetActor)
	{
		return false;
	}

	if (bRequireLineOfSightForDirectPursuit && !AIController->LineOfSightTo(TargetActor))
	{
		return false;
	}

	const FVector ZombieLocation = ZombieCharacter->GetActorLocation();
	const FVector TargetLocation = GetClosestPointOnChaseTarget(TargetActor, ZombieLocation);
	const float Distance2D = FVector::Dist2D(ZombieLocation, TargetLocation);
	const float HeightDelta = TargetLocation.Z - ZombieLocation.Z;

	if (Distance2D > DirectPursuitRange2D)
	{
		return false;
	}

	if (HeightDelta > DirectPursuitMaxRise)
	{
		return false;
	}

	if (HeightDelta < -DirectPursuitMaxDrop)
	{
		return false;
	}

	return true;
}

void UBTTask_ChaseHuman::ApplyDirectPursuitFallback(AAIController* AIController, ABaseZombie* ZombieCharacter, AActor* TargetActor) const
{
	if (!AIController || !ZombieCharacter || !TargetActor)
	{
		return;
	}

	AIController->SetFocus(TargetActor);
		ZombieCharacter->ApplyDirectPursuitInput(GetClosestPointOnChaseTarget(TargetActor, ZombieCharacter->GetActorLocation()));
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
	if (TryUseFallZonePursuit(AIController, ZombieCharacter, TargetActor))
	{
		return EBTNodeResult::InProgress;
	}

	bUsingFallZonePursuit = false;

	const bool bMoveRequested = RequestChaseMove(AIController, ZombieCharacter, TargetActor, true);
	if (!bMoveRequested && ShouldUseDirectPursuitFallback(AIController, ZombieCharacter, TargetActor))
	{
		ApplyDirectPursuitFallback(AIController, ZombieCharacter, TargetActor);
	}

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

	if (TryUseFallZonePursuit(AIController, ZombieCharacter, TargetActor))
	{
		return;
	}

	if (bUsingFallZonePursuit)
	{
		bUsingFallZonePursuit = false;
		ResetMoveRequestState();
	}

	const FVector TargetReachPoint = GetClosestPointOnChaseTarget(TargetActor, ZombieCharacter->GetActorLocation());
	const float Distance = FVector::Dist(ZombieCharacter->GetActorLocation(), TargetReachPoint);

	if (Distance <= StopDistance)
	{
		AIController->StopMovement();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	const bool bMoveRequested = RequestChaseMove(AIController, ZombieCharacter, TargetActor, false);
	if (!bMoveRequested && ShouldUseDirectPursuitFallback(AIController, ZombieCharacter, TargetActor))
	{
		ApplyDirectPursuitFallback(AIController, ZombieCharacter, TargetActor);
	}
}
