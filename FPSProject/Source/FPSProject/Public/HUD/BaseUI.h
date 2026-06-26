// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BaseUI.generated.h"

/**
 * 
 */
class UTextBlock;	// 전방 선언
class UCanvasPanel;

UCLASS()
class FPSPROJECT_API UBaseUI : public UUserWidget
{
	GENERATED_BODY()

	/*
	meta= (BindWidget) :UMG위젯 자동 바인딩, C++ 멤버 변수와 UMG 위젯을 연결.
	*/

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	//UTextBlock* TimerText; << 이건 옛날방법
	TObjectPtr<UTextBlock>TimerText; //UE5에선 이 방식 사용

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta=(BindWidgetAnim, AllowPrivateAccess), Transient)
	/*
	Transient < 이 속성은 객체가 직렬화되지 않도록 지정, 즉 저장이나 로드 시에 이 변수의 값이 포함되지 않음. => 애니 데이터 저장x라 로딩시 항상 새로 초기화 됨.
	*/
	UWidgetAnimation* TCountVibration;	//변수 이름은 애니 이름과 동일해야함.

	UPROPERTY(meta = (BindWidgetAnim, AllowPrivateAccess), Transient)
	UWidgetAnimation* TPopUp;	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundWave* Boom;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound")
	USoundWave* ClockS;

public:
	UFUNCTION(BlueprintCallable)
	void SetRemainingTime(int32 NewRemainingTime);

	void FinishTruckLoadingPhase();
	void PlayAni_Vibration() {PlayAnimation(TCountVibration);}
	void PlayAni_PopUp() { PlayAnimation(TPopUp); }

private:
	void UpdateTimerText();
	void HandleTimerFinished();

	int32 TotalTime = 30;
	int32 LastDisplayedTime = INDEX_NONE;
	bool bHasFinishedLoadingPhase = false;
};
