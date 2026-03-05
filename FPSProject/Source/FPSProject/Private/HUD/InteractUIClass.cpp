// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/InteractUIClass.h"
#include "Components/TextBlock.h"
//
void UInteractUIClass::NativeConstruct()
{
	Super::NativeConstruct();
	// 애니메이션이랑 텍스트 블록이 제대로 바인딩 되었는지 확인
	InteractText->SetIsEnabled(false);	//상호작용 안되게
}
void UInteractUIClass::PlayAni_PopUp(bool bOpen)
{
	if (!PopUp) return;
	PlayAnimation(PopUp);

	//애니메이션의 마지막 프레임이 띄워져있기때문에 알아서 유지가 됨.
	InteractText->SetText(FText::FromString(bOpen ? TEXT("문 열기") : TEXT("close")));
}

void UInteractUIClass::RePlayAni_PopUp() // 범위 벗어나서 사라짐.
{
	if (PopUp)
	{
		PlayAnimation(PopUp, 0.f, 1, EUMGSequencePlayMode::Reverse); //애니메이션 닫기
	}
}
void UInteractUIClass:: SetDoorOpenState(bool bIsOpen)
{
	if (InteractText)
	{
		//InteractText->SetText(FText::);
		InteractText->SetText(FText::FromString(TEXT("open")));
	}
}