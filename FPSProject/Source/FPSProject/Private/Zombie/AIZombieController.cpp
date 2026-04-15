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
            PlayerCharacter->CurrentTruck &&
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

    // 매 프레임 플레이어를 찾아서 Blackboard에 저장
    PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

    if (PlayerPawn && GetBlackboardComponent())
    {
        AActor* TargetActor = ResolveZombieTarget(PlayerPawn);
        if (!TargetActor)
        {
            ClearFocus(EAIFocusPriority::Gameplay);
            GetBlackboardComponent()->ClearValue(TargetPlayerKey);
            return;
        }

        // 시야 체크 
        bool bCanSee = LineOfSightTo(PlayerPawn);
        if (TargetActor != PlayerPawn)
        {
            bCanSee = bCanSee || LineOfSightTo(TargetActor);
        }

        if (bCanSee && GetPawn())
        {
            FVector Forward = GetPawn()->GetActorForwardVector();
            FVector TargetDir = (TargetActor->GetActorLocation() - GetPawn()->GetActorLocation()).GetSafeNormal();
            float DotProduct = FVector::DotProduct(Forward, TargetDir);
            if (DotProduct < 0.7f)
            {
                bCanSee = false;
            }
        }

        if (bCanSee)
        {
            // 플레이어가 트럭에 타고 있으면 트럭에 시선 고정.
            SetFocus(TargetActor);
            // 플레이어 또는 플레이어가 탄 트럭을 블랙보드에 저장 → 행동트리가 이걸 보고 추적/공격
            GetBlackboardComponent()->SetValueAsObject(TargetPlayerKey, TargetActor);
			GetBlackboardComponent()->SetValueAsVector(FName("PlayerLocation"), TargetActor->GetActorLocation());
            
        }
        else
        {
			ClearFocus(EAIFocusPriority::Gameplay);
            // 시야 밖이면 타겟 지우기 → 행동트리가 대기 상태
            GetBlackboardComponent()->ClearValue(TargetPlayerKey);
        }
    }
}
