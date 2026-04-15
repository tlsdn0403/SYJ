// Fill out your copyright notice in the Description page of Project Settings.

#include "HUD/BaseUI.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"

/*
NativeOnInitialized : 위젯이 생성될 때 딱 한 번 호출,  에디터 편집 시에도 생성될 때 호출
NativeConstruct : AddToViewport 시 호출, Viewport 에 Add 될 때마다 호출됨
NativeDestruct : RemoveFromParent(RemoveFromViewport) 시 호출, Viewport 에서 제거될 때마다 호출됨
*/

void UBaseUI::NativeConstruct()
{
	Super::NativeConstruct(); // 부모 클래스의 NativeConstruct 호출

	if (TimerText)
	{
		TimerText->SetText(FText::FromString(TEXT("05:00")));
		/*
			SetText()는 FText만 받기 때문에
		*/
		GetWorld()->GetTimerManager().SetTimer(TimerHandle,this,&UBaseUI::UpdateTimer,1.0f,true);
	}
}


void UBaseUI:: UpdateTimer()
{
	totalTime -= 1;
	if(totalTime < 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
		return;
	}
	if (totalTime <= 5) {
		PlayAni_PopUp();
	}
	else if (totalTime <= 20)	//20초 이하일 때 진동 애니메이션 실행
	{
		PlayAni_Vibration();
	}

	FString Stime = FString::Printf(TEXT("%02d:%02d"), totalTime/60,totalTime%60);
	TimerText->SetText(FText::FromString(Stime));
}
