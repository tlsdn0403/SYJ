// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FPSBaseCharacter.generated.h"

UCLASS()
class FPSPROJECT_API AFPSBaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AFPSBaseCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;


    // 앞으로 이동 및 뒤로 이동 입력을 처리합니다.
    UFUNCTION()
    void MoveForward(float Value);

    // 오른쪽 이동 및 왼쪽 이동 입력을 처리합니다.
    UFUNCTION()
    void MoveRight(float Value);

    // 키가 눌릴 경우 점프 플래그를 설정합니다.
    UFUNCTION()
    void StartJump();

    // 키가 떼어질 경우 점프 플래그를 지웁니다.
    UFUNCTION()
    void StopJump();
};
