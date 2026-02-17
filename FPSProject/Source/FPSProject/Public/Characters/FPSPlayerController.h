// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FPSPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class FPSPROJECT_API AFPSPlayerController : public APlayerController
{
	GENERATED_BODY()
	

protected:
   // virtual void SetupInputComponent() override;

   // // 앞으로 이동 및 뒤로 이동 입력을 처리
   // void MoveForward(float Value);

   // // 오른쪽 이동 및 왼쪽 이동 입력을 처리
   // void MoveRight(float Value);

   // // 키가 눌릴 경우 점프 플래그를 설정
   // void StartJump();

   // // 키가 떼어질 경우 점프 플래그를 지움
   // void StopJump();

   // void Fire();
   //// void OnPress1();
};
