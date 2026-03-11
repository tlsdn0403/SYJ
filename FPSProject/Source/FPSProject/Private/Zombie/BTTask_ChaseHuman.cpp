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

	// TickTask 활성화 (매 틱마다 실행)
    bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_ChaseHuman::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIController = OwnerComp.GetAIOwner();  //AI 컨트롤러 가져오기
	ACharacter* ZombieCharacter = Cast<ACharacter>(AIController->GetPawn()); // 좀비 캐릭터 가져오기
    AActor* TargetActor = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetPlayerKey.SelectedKeyName));   // Blackboard에서 플레이어 타겟 가져오기
    if (!TargetActor|| !AIController || !ZombieCharacter)
    {
        return EBTNodeResult::Failed; // 하나라도 없으면 실패 반환
    }


    UE_LOG(LogTemp, Warning, TEXT("Zombie chasing player at location: %s"), *TargetActor->GetActorLocation().ToString());

    // 이동 중이므로 In Progress 반환 (계속 실행)
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

    ACharacter* ZombieCharacter = Cast<ACharacter>(AIController->GetPawn());
    AActor* TargetActor = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetPlayerKey.SelectedKeyName));

    if (!ZombieCharacter || !TargetActor)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    float Distance = FVector::Dist(ZombieCharacter->GetActorLocation(), TargetActor->GetActorLocation());

    // 공격 범위에 도달하면 성공 반환 (공격으로 전환)
    if (Distance <= StopDistance)
    {
        UE_LOG(LogTemp, Warning, TEXT("In attack range! Distance: %f"), Distance);
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }

    // 매 틱마다 플레이어 위치 업데이트
    AIController->MoveToActor(TargetActor, StopDistance);

    UE_LOG(LogTemp, Log, TEXT("Chasing... Distance: %f"), Distance);
}
