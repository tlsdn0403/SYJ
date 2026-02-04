// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/AIZombieController.h"
#include "Kismet/GameplayStatics.h"
void AAIZombieController::BeginPlay()
{
	Super::BeginPlay();

	PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(),0.f);
	SetFocus(PlayerPawn);
}

void AAIZombieController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0.f);
	MoveToActor(PlayerPawn, 150.f);
}
