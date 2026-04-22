// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "StartModeController.generated.h"

/**
 * 
 */

class UStartScreenClass;


UCLASS()
class FPSPROJECT_API AStartModeController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;
public:
	// 시작화면 위젯
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StartScreen")
	TSubclassOf<UStartScreenClass> StartScreenWidgetClass;

	UPROPERTY()
	UStartScreenClass* StartSW;

	UFUNCTION()
	void StartGame();
};
