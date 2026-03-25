// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/InteractUIClass.h"
#include "Components/TextBlock.h"
//
void UInteractUIClass::NativeConstruct()
{
	Super::NativeConstruct();
	// 애니메이션이랑 텍스트 블록이 제대로 바인딩 되었는지 확인
	InteractText->SetIsEnabled(true);
	//InteractText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 1.f)));
	InteractText->SetText(FText::FromString(TEXT("Open")));
}
void UInteractUIClass::PlayAni_PopUp(bool bOpen)
{
	if (!PopUp) return;
	PlayAnimation(PopUp);

	//애니메이션의 마지막 프레임이 띄워져있기때문에 알아서 유지가 됨.
}

void UInteractUIClass::RePlayAni_PopUp() // 범위 벗어나서 사라짐.
{
	if (PopUp)
	{
		PlayAnimation(PopUp, 0.f, 1, EUMGSequencePlayMode::Reverse); //애니메이션 닫기
	}
}

void UInteractUIClass::SetInteractText(const FText& NewText)
{
	if (InteractText)
	{
		InteractText->SetText(NewText);
	}
}

void UInteractUIClass::SetDoorInteractText(const bool bOpen)
{
	//InteractText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 1.f)));
	InteractText->SetText(FText::FromString(bOpen ? TEXT("Close") : TEXT("Open")));
}
