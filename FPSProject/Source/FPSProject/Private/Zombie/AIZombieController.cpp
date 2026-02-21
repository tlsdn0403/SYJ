// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/AIZombieController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"


const FName AAIZombieController::TargetPlayerKey = FName("TargetPlayer");


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

    // 매 프레임 플레이어를 찾아서 Blackboard에 저장
    PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

    if (PlayerPawn && GetBlackboardComponent())
    {
        // 시야 체크 
        bool bCanSee = LineOfSightTo(PlayerPawn);

        if (bCanSee && GetPawn())
        {
            FVector Forward = GetPawn()->GetActorForwardVector();
            FVector TargetDir = (PlayerPawn->GetActorLocation() - GetPawn()->GetActorLocation()).GetSafeNormal();
            float DotProduct = FVector::DotProduct(Forward, TargetDir);
            if (DotProduct < 0.7f)
            {
                bCanSee = false;
            }
        }

        if (bCanSee)
        {
            // 플레이어를 블랙보드에 저장 → 행동트리가 이걸 보고 추적/공격
            GetBlackboardComponent()->SetValueAsObject(TargetPlayerKey, PlayerPawn);
			GetBlackboardComponent()->SetValueAsVector(FName("PlayerLocation"), PlayerPawn->GetActorLocation());
        }
        else
        {
            // 시야 밖이면 타겟 지우기 → 행동트리가 대기 상태
            GetBlackboardComponent()->ClearValue(TargetPlayerKey);
        }
    }
}
