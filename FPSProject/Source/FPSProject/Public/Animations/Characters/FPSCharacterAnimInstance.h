// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animations/FPSBaseAnimInstance.h"
#include "FPSCharacterAnimInstance.generated.h"

/**
 * 
 */
class AFPSBaseCharacter;
class UCharacterMovementComponent;
UCLASS()
class FPSPROJECT_API UFPSCharacterAnimInstance : public UFPSBaseAnimInstance
{
	GENERATED_BODY()
	
public:
	virtual void NativeInitializeAnimation()override;

	//게임 스레드가 아닌 워커 스레드에서 작동함
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY()
	AFPSBaseCharacter* OwningCharacter;

	UPROPERTY()
	UCharacterMovementComponent* OwningMovementComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animdata|LocalmotionData")
	float GroundSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animdata|LocalmotionData")
	bool bHasAcceleration;  //가속도가 있는지 여부
};
