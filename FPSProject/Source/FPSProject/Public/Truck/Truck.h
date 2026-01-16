// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Truck.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API ATruck : public AWheeledVehiclePawn
{
	GENERATED_BODY()

protected:
	//트럭 이동을 위한 것.
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void Brake(float Value);
};
