// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/StartScreenClass.h"
#include "Components/EditableText.h"
#include "Components/Button.h"
#include "FPSProjectGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Protocol.pb.h"
#include "ClientPacketHandler.h"

void UStartScreenClass::NativeConstruct()
{
	Super::NativeConstruct();

	if (LoginButton)
	{
		LoginButton->OnClicked.AddDynamic(this, &UStartScreenClass::OnClickLogin);
	}
}

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

void UStartScreenClass::OnClickLogin()
{
	PlayAni_Click();

	FString ServerIP = IPT->GetText().ToString();
	FString UserID = NicknameT->GetText().ToString();

	if (ServerIP.IsEmpty())
	{
		ServerIP = TEXT("127.0.0.1");
	}

	if (auto* GameInstance = Cast<UFPSProjectGameInstance>(UGameplayStatics::GetGameInstance(GetWorld())))
	{
		GameInstance->ConnectToGameServer(ServerIP);
		UE_LOG(LogTemp, Warning, TEXT("[Login UI] 접속 시도! IP: %s / Nickname: %s"), *ServerIP, *UserID);

		Protocol::C_LOGIN LoginPkt;

		LoginPkt.set_nickname(TCHAR_TO_UTF8(*UserID));

		SendBufferRef SendBuffer = ClientPacketHandler::MakeSendBuffer(LoginPkt);
		GameInstance->SendPacket(SendBuffer);

		this -> RemoveFromParent();

		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = false;
		}
	}
}
