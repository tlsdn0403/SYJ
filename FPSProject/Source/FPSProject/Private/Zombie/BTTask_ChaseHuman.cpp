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
}

void UBTTask_ChaseHuman::RequestChaseMove(AAIController* AIController, ABaseZombie* ZombieCharacter, AActor* TargetActor, bool bForceRequest)
{
	if (!AIController || !ZombieCharacter || !TargetActor)
	{
		return;
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
		return;
	}

	const EPathFollowingRequestResult::Type RequestResult =
		AIController->MoveToActor(TargetActor, StopDistance, true, true, true, nullptr, true);

	if (RequestResult != EPathFollowingRequestResult::Failed)
	{
		LastMoveRequestTime = CurrentTime;
		LastIssuedTargetActor = TargetActor;
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
	RequestChaseMove(AIController, ZombieCharacter, TargetActor, true);

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

	RequestChaseMove(AIController, ZombieCharacter, TargetActor, false);
}
