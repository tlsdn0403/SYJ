// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/AIZombieController.h"
#include "Zombie/BaseZombie.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Truck/Truck.h"


const FName AAIZombieController::TargetPlayerKey = FName("TargetPlayer");

namespace
{
    AActor* ResolveZombieTarget(APawn* PlayerPawn)
    {
        AFPSBaseCharacter* PlayerCharacter = Cast<AFPSBaseCharacter>(PlayerPawn);
        if (PlayerCharacter &&
            IsValid(PlayerCharacter->CurrentTruck) &&
            (PlayerCharacter->IsDrivingTruck() ||
                PlayerCharacter->IsOnTruckCargo() ||
                PlayerCharacter->IsUsingMountedWeapon()))
        {
            return PlayerCharacter->CurrentTruck;
        }

        return PlayerPawn;
    }
}

void AAIZombieController::BeginPlay()
{
	Super::BeginPlay();
	PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0.f);

	if(ZombieBehaviorTree)
	{
		RunBehaviorTree(ZombieBehaviorTree);
	}
}

void AAIZombieController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (ABaseZombie* Zombie = Cast<ABaseZombie>(GetPawn()))
    {
        if (!Zombie->IsAlive())
        {
            ClearFocus(EAIFocusPriority::Gameplay);

            if (GetBlackboardComponent())
            {
                GetBlackboardComponent()->ClearValue(TargetPlayerKey);
            }

            return;
        }
    }

    UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();
    if (!BlackboardComponent)
    {
        return;
    }

    PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!PlayerPawn)
    {
        ClearFocus(EAIFocusPriority::Gameplay);
        BlackboardComponent->ClearValue(TargetPlayerKey);
        bHasKnownTarget = false;
        return;
    }

    AActor* TargetActor = ResolveZombieTarget(PlayerPawn);
    if (!TargetActor)
    {
        ClearFocus(EAIFocusPriority::Gameplay);
        BlackboardComponent->ClearValue(TargetPlayerKey);
        bHasKnownTarget = false;
        return;
    }

    const bool bTargetIsTruck = TargetActor != PlayerPawn;
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    bool bCanDetectTarget = false;
    if (bTargetIsTruck)
    {
        const float DistanceSq = GetPawn()
            ? FVector::DistSquared(GetPawn()->GetActorLocation(), TargetActor->GetActorLocation())
            : 0.0f;

        // 트럭은 크고 소리가 나는 목표라서, 앞 좀비에게 시야가 가려져도 일정 거리 안에서는 계속 추적한다.
        bCanDetectTarget =
            DistanceSq <= FMath::Square(TruckAwarenessDistance) ||
            LineOfSightTo(TargetActor) ||
            LineOfSightTo(PlayerPawn);
    }
    else
    {
        bCanDetectTarget = LineOfSightTo(PlayerPawn);

        if (bCanDetectTarget && GetPawn())
        {
            const FVector Forward = GetPawn()->GetActorForwardVector();
            const FVector TargetDir = (TargetActor->GetActorLocation() - GetPawn()->GetActorLocation()).GetSafeNormal();
            const float DotProduct = FVector::DotProduct(Forward, TargetDir);
            if (DotProduct < SightDotThreshold)
            {
                bCanDetectTarget = false;
            }
        }
    }

    if (bCanDetectTarget)
    {
        bHasKnownTarget = true;
        LastTargetSeenTime = CurrentTime;
        LastKnownTargetLocation = TargetActor->GetActorLocation();
    }

    const float MemoryDuration = bTargetIsTruck ? TruckTargetMemoryDuration : TargetMemoryDuration;
    const bool bCanUseMemory = bHasKnownTarget && (CurrentTime - LastTargetSeenTime) <= MemoryDuration;

    if (bCanDetectTarget || bCanUseMemory)
    {
        SetFocus(TargetActor);
        BlackboardComponent->SetValueAsObject(TargetPlayerKey, PlayerPawn);
        BlackboardComponent->SetValueAsVector(
            FName("PlayerLocation"),
            bCanDetectTarget ? TargetActor->GetActorLocation() : LastKnownTargetLocation);
    }
    else
    {
        ClearFocus(EAIFocusPriority::Gameplay);
        BlackboardComponent->ClearValue(TargetPlayerKey);
    }
}
