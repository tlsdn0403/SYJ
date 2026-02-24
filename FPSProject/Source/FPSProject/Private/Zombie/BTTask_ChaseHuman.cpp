// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/BTTask_ChaseHuman.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"


UBTTask_ChaseHuman::UBTTask_ChaseHuman()
{
	NodeName = TEXT("Chase Human");
	// Blackboard 키 초기화
	TargetPlayerKey.SelectedKeyName = FName("TargetPlayer");
}

EBTNodeResult::Type UBTTask_ChaseHuman::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        return EBTNodeResult::Failed;
    }

    ACharacter* ZombieCharacter = Cast<ACharacter>(AIController->GetPawn());
    if (!ZombieCharacter)
    {
        return EBTNodeResult::Failed;
    }

    // Blackboard에서 플레이어 타겟 가져오기
    AActor* TargetActor = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetPlayerKey.SelectedKeyName));
    if (!TargetActor)
    {
        return EBTNodeResult::Failed; // 타겟이 없으면 실패
    }

    // 좀비를 플레이어 쪽으로 이동시키기 (AIMoveTo 사용)
    AIController->MoveToActor(TargetActor, 100.0f); // 100cm 거리까지 접근

    UE_LOG(LogTemp, Warning, TEXT("Zombie chasing player at location: %s"), *TargetActor->GetActorLocation().ToString());

    // 이동 중이므로 In Progress 반환 (계속 실행)
    return EBTNodeResult::InProgress;
}
