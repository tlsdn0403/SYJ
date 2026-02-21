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

	UPROPERTY(EditAnyWhere)
	class UBehaviorTree* ZombieBehaviorTree;

	// 블랙보드 키 이름
	static const FName TargetPlayerKey;

public:
	APawn* PlayerPawn;

};
