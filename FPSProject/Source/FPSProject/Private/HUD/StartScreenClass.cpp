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

	// 로그인 버튼을 클릭하면 OnClickLogin 함수가 실행되도록 연결
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

	// UI 텍스트 박스에서 글자 빼오기
	FString ServerIP = IPT->GetText().ToString();
	FString UserID = NicknameT->GetText().ToString();

	// IP를 안 적었을 경우 기본 로컬 IP 설정
	if (ServerIP.IsEmpty())
	{
		ServerIP = TEXT("127.0.0.1");
	}

	if (auto* GameInstance = Cast<UFPSProjectGameInstance>(UGameplayStatics::GetGameInstance(GetWorld())))
	{
		// 서버 접속 시도
		GameInstance->ConnectToGameServer(ServerIP);
		UE_LOG(LogTemp, Warning, TEXT("[Login UI] 접속 시도! IP: %s / Nickname: %s"), *ServerIP, *UserID);

		// C_LOGIN 패킷 생성 및 닉네임 세팅
		Protocol::C_LOGIN LoginPkt;

		// [매우 중요] 언리얼의 FString을 프로토콜 버퍼용 std::string(UTF-8)으로 변환!
		// (.proto 파일의 변수명이 nickname이 아니라면 set_이름() 부분을 알맞게 고쳐줘)
		LoginPkt.set_nickname(TCHAR_TO_UTF8(*UserID));

		// 패킷 전송 (MakeSendBuffer로 굽고 GameInstance를 통해 쏘기)
		SendBufferRef SendBuffer = ClientPacketHandler::MakeSendBuffer(LoginPkt);
		GameInstance->SendPacket(SendBuffer);

		// 로그인 UI를 화면에서 깔끔하게 제거!
		this->RemoveFromParent();

		// 마우스 커서를 숨기고 게임 조작 모드로 되돌리기!
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = false;
		}
	}
}