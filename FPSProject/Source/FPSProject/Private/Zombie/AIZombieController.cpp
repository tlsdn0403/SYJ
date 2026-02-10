// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/AIZombieController.h"
#include "Kismet/GameplayStatics.h"


void AAIZombieController::BeginPlay()
{
	Super::BeginPlay();
}

void AAIZombieController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0.f);

	bool IsLineOfSightTo = LineOfSightTo(PlayerPawn);

	if (IsLineOfSightTo)
	{
		MoveToActor(PlayerPawn, 150.f);
		SetFocus(PlayerPawn);
	}
	else
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		StopMovement();
	}
	
}
