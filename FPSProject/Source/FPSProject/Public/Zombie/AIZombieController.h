// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AIZombieController.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API AAIZombieController : public AAIController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;


public:
	APawn* PlayerPawn;

private:
	UPROPERTY(EditAnyWhere)
	class UBehaviorTree* AIBhaviorTree;
};
