// Fill out your copyright notice in the Description page of Project Settings.


#include "Zombie/AIZombieController.h"
#include "Kismet/GameplayStatics.h"
void AAIZombieController::BeginPlay()
{
	Super::BeginPlay();

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(),0.f);
	SetFocus(PlayerPawn);
}
