// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/BTTask_Attack.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/FPSBaseCharacter.h"
#include "Truck/Truck.h"
#include "Zombie/BaseZombie.h"

namespace
{
	AActor* ResolveAttackTarget(AActor* BlackboardTarget)
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
}

UBTTask_Attack::UBTTask_Attack()
{
}

bool UBTTask_Attack::ShouldSkipMovingTruckAttack(AActor* TargetActor) const
{
	const ATruck* Truck = Cast<ATruck>(TargetActor);
	return Truck &&
		MovingTruckAttackSpeedThreshold > 0.0f &&
		Truck->GetVelocity().SizeSquared2D() >= FMath::Square(MovingTruckAttackSpeedThreshold);
}

EBTNodeResult::Type UBTTask_Attack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);


	if(!OwnerComp.GetAIOwner())
	{
		return EBTNodeResult::Failed;
	}
	
	ABaseZombie* Zombie = Cast<ABaseZombie>(OwnerComp.GetAIOwner()->GetPawn());

	if(!Zombie)
	{
		return EBTNodeResult::Failed;
	}

	if (Zombie->IsAttacking())
	{
		return EBTNodeResult::Succeeded;
	}

	AActor* TargetActor = nullptr;
	if (UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent())
	{
		AActor* BlackboardTarget = Cast<AActor>(BlackboardComponent->GetValueAsObject(FName("TargetPlayer")));
		TargetActor = ResolveAttackTarget(BlackboardTarget);
	}

	if (ShouldSkipMovingTruckAttack(TargetActor))
	{
		return EBTNodeResult::Failed;
	}

	if (!Zombie->IsTargetInAttackRange(TargetActor))
	{
		return EBTNodeResult::Failed;
	}

	Zombie->Attack(TargetActor);

	return EBTNodeResult::Succeeded;
}
