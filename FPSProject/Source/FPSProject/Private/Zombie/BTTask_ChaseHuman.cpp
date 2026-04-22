// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/BTTask_ChaseHuman.h"
#include "Zombie/BaseZombie.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Truck/Truck.h"

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

    FVector GetMovePointOnNavigation(AActor* TargetActor, const FVector& FromLocation)
    {
        const FVector TargetReachPoint = GetClosestPointOnTarget(TargetActor, FromLocation);
        if (!TargetActor)
        {
            return TargetReachPoint;
        }

        if (UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(TargetActor))
        {
            FNavLocation ProjectedLocation;
            const FVector QueryExtent(300.0f, 300.0f, 300.0f);
            if (NavSystem->ProjectPointToNavigation(TargetReachPoint, ProjectedLocation, QueryExtent))
            {
                return ProjectedLocation.Location;
            }
        }

        return TargetReachPoint;
    }
}

UBTTask_ChaseHuman::UBTTask_ChaseHuman()
{
	NodeName = TEXT("Chase Human");
	TargetPlayerKey.SelectedKeyName = FName("TargetPlayer");
    bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_ChaseHuman::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
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

    UE_LOG(LogTemp, Warning, TEXT("Zombie chasing target at location: %s"), *TargetActor->GetActorLocation().ToString());

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

    const FVector TargetReachPoint = GetClosestPointOnTarget(TargetActor, ZombieCharacter->GetActorLocation());
    const float Distance = FVector::Dist(ZombieCharacter->GetActorLocation(), TargetReachPoint);

    if (Distance <= StopDistance)
    {
        UE_LOG(LogTemp, Warning, TEXT("In attack range! Distance: %f"), Distance);
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }

    const FVector MoveGoalLocation = GetMovePointOnNavigation(TargetActor, ZombieCharacter->GetActorLocation());
    AIController->MoveToLocation(MoveGoalLocation, StopDistance);

    UE_LOG(LogTemp, Log, TEXT("Chasing... Distance: %f"), Distance);
}
