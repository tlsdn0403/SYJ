// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingUI.generated.h"

/**
 *
 */

class UImage;
class UTextBlock;

// 플레이어 접속에 따라서 빛남


UCLASS()
class FPSPROJECT_API ULoadingUI : public UUserWidget
{
	GENERATED_BODY()
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	FTimerHandle LoadingDotTimerHandle;

	int32 DotCount = 0;

	void UpdateLoadingText();

public:
	UPROPERTY(meta = (BindWidget))
	UImage* Player1;
	UPROPERTY(meta = (BindWidget))	// 플레이어 접속에 따라서 빛남
	UImage* Player2;
	UPROPERTY(meta = (BindWidget))	//
	UImage* Player3;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* loadingmemo;


	UPROPERTY()
	TArray<UImage*> Players;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TArray<FText> setText;

	int32 CurrentTextIndex = 0;

	void connect(int num);		//플레이어 접속하면 사람에 불켜짐
	void logout();		//나가면 다시 불꺼져

	int OnlineP = 0;	//현재 접속 중인 플레이어 수
};
