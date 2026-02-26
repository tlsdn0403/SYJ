// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/BTTask_Attack.h"
#include "AIController.h"
#include "Zombie/BaseZombie.h"

UBTTask_Attack::UBTTask_Attack()
{
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
	Zombie->Attack();

	return EBTNodeResult::Type();
}
