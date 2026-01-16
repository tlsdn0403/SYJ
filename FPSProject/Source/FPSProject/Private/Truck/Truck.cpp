// Fill out your copyright notice in the Description page of Project Settings.


#include "Truck/Truck.h"
#include "ChaosWheeledVehicleMovementComponent.h"

void ATruck::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	PlayerInputComponent->BindAxis("Throttle", this, &ATruck::MoveForward);   // 가속/후진
	PlayerInputComponent->BindAxis("Steer", this, &ATruck::MoveRight);       // 핸들 좌우
	PlayerInputComponent->BindAxis("Brake", this, &ATruck::Brake);           // 브레이크
}

void ATruck::MoveForward(float Value)
{
	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetThrottleInput(Value);
	}
}

void ATruck::MoveRight(float Value)
{
	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetSteeringInput(Value);
	}
}

void ATruck::Brake(float Value)
{
	if (auto* MoveComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()))
	{
		MoveComp->SetBrakeInput(Value);
	}
}