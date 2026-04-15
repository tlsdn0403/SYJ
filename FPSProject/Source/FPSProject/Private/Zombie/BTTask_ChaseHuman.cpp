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
        if (TargetActor)
        {
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

        return FromLocation;
    }
}


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
	ABaseZombie* ZombieCharacter = AIController ? Cast<ABaseZombie>(AIController->GetPawn()) : nullptr; // 좀비 캐릭터 가져오기
    AActor* BlackboardTarget = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetPlayerKey.SelectedKeyName));   // Blackboard에서 플레이어 타겟 가져오기
    AActor* TargetActor = ResolveChaseTarget(BlackboardTarget);
    if (!TargetActor || !AIController || !ZombieCharacter || !ZombieCharacter->IsAlive())
    {
        return EBTNodeResult::Failed; // 하나라도 없으면 실패 반환
    }


    UE_LOG(LogTemp, Warning, TEXT("Zombie chasing target at location: %s"), *TargetActor->GetActorLocation().ToString());

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

    ABaseZombie* ZombieCharacter = Cast<ABaseZombie>(AIController->GetPawn());
    AActor* BlackboardTarget = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(TargetPlayerKey.SelectedKeyName));
    AActor* TargetActor = ResolveChaseTarget(BlackboardTarget);

    if (!ZombieCharacter || !TargetActor || !ZombieCharacter->IsAlive())
    {
        AIController->StopMovement();
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    const FVector TargetReachPoint = GetClosestPointOnTarget(TargetActor, ZombieCharacter->GetActorLocation());
    float Distance = FVector::Dist(ZombieCharacter->GetActorLocation(), TargetReachPoint);

    // 공격 범위에 도달하면 성공 반환 (공격으로 전환)
    if (Distance <= StopDistance)
    {
        UE_LOG(LogTemp, Warning, TEXT("In attack range! Distance: %f"), Distance);
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }

    // 매 틱마다 타겟 위치 업데이트
    AIController->MoveToActor(TargetActor, StopDistance);

    UE_LOG(LogTemp, Log, TEXT("Chasing... Distance: %f"), Distance);
}
