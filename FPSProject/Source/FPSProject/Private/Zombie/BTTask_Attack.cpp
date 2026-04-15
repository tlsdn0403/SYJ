// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/BTTask_Attack.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
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
	AActor* TargetActor = nullptr;
	if (UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent())
	{
		TargetActor = Cast<AActor>(BlackboardComponent->GetValueAsObject(FName("TargetPlayer")));
	}

	Zombie->Attack(TargetActor);

	return EBTNodeResult::Succeeded;
}
