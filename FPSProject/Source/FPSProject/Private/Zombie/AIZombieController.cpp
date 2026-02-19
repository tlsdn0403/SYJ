// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/AIZombieController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"


void AAIZombieController::BeginPlay()
{
	Super::BeginPlay();
	PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0.f);

	if(AIBhaviorTree)
	{
		RunBehaviorTree(AIBhaviorTree);
	}
	// 플레이어 위치정보 키값에 저장
	GetBlackboardComponent()->SetValueAsVector(TEXT("PlayerLocation"),PlayerPawn->GetActorLocation());

	// 좀비 위치정보 키값에 저장
	GetBlackboardComponent()->SetValueAsVector(TEXT("PawnLocation"), GetPawn()->GetActorLocation());

}

void AAIZombieController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	bool IsLineOfSightTo = LineOfSightTo(PlayerPawn);

	FVector Forward = GetPawn()->GetActorForwardVector();
	FVector TargetDir = (PlayerPawn->GetActorLocation() - GetPawn()->GetActorLocation()).GetSafeNormal();

	// 두 벡터의 내적 계산
	float DotProduct = FVector::DotProduct(Forward, TargetDir);

	// 시야각이 90도 이상인 경우 0.707이 90도에서의 내적값이므로, 0.7로 약간 여유를 둠
	if (DotProduct < 0.7f) 
	{
		IsLineOfSightTo = false;
	}

	if (IsLineOfSightTo)
	{
		GetBlackboardComponent()->SetValueAsVector(TEXT("PlayerLocation"), PlayerPawn->GetActorLocation());

		SetFocus(PlayerPawn);
	}
	else
	{
		GetBlackboardComponent()->ClearValue(TEXT("PlayerLocation"));
		StopMovement();
	}
	
}
