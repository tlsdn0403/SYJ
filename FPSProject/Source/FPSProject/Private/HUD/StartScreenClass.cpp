// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/StartScreenClass.h"
#include "Components/Button.h"

void UStartScreenClass::NativeConstruct()
{
	Super::NativeConstruct();

}

//bool UStartScreenClass::Initialize()
//{
//    Super::Initialize();
//
//    if (LoginButton)
//    {
//        LoginButton->OnClicked.AddDynamic(this, &UStartScreenClass::OnClickLogin);
//    }
//
//    return true;
//}
//
//void UStartScreenClass::OnClickLogin()
//{
//    UE_LOG(LogTemp, Warning, TEXT("로그인 버튼 클릭됨"));
//}


void UStartScreenClass::PlayAni_Start() {
	//그 다음 애니메이션과 연결 후 실행
	FWidgetAnimationDynamicEvent EndDelegate;
	EndDelegate.BindDynamic(this, &UStartScreenClass::PlayAni_Click);
	BindToAnimationFinished(StartPop, EndDelegate);

	PlayAnimation(StartPop); 
}
void UStartScreenClass::PlayAni_Click() { 
	PlayAnimation(Click); 
}