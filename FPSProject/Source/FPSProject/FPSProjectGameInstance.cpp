// Fill out your copyright notice in the Description page of Project Settings.


#include "FPSProjectGameInstance.h"
#include "Sockets.h"
#include "Common/TcpSocketBuilder.h"
#include "Serialization/ArrayWriter.h"
#include "SocketSubsystem.h"
#include "PacketSession.h"
#include "Protocol.pb.h"
#include "ClientPacketHandler.h"
#include "Characters/FPSBaseCharacter.h"
#include "Kismet/GameplayStatics.h"

void UFPSProjectGameInstance::ConnectToGameServer()
{
	Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(TEXT("Stream"), TEXT("Client Socket"));

	FIPv4Address Ip;
	FIPv4Address::Parse(IpAddress, Ip);

	TSharedRef<FInternetAddr> InternetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	InternetAddr->SetIp(Ip.Value);
	InternetAddr->SetPort(Port);

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Connecting To Server...")));

	bool Connected = Socket->Connect(*InternetAddr);

	if (Connected)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Connection Success")));

		// Session
		GameServerSession = MakeShared<PacketSession>(Socket);
		GameServerSession->Run();

		// TEMP : Lobby에서 캐릭터 선택창 등
		{
			Protocol::C_LOGIN Pkt;
			SendBufferRef SendBuffer = ClientPacketHandler::MakeSendBuffer(Pkt);
			SendPacket(SendBuffer);
		}
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Connection Failed")));
	}
}

void UFPSProjectGameInstance::DisconnectFromGameServer()
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	Protocol::C_LEAVE_GAME LeavePkt;
	SEND_PACKET(LeavePkt);

	Socket->Close();

	GameServerSession.Reset();

	/*if (Socket)
	{
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
		SocketSubsystem->DestroySocket(Socket);
		Socket = nullptr;
	}*/
}

void UFPSProjectGameInstance::HandleRecvPackets()
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	GameServerSession->HandleRecvPackets();
}

void UFPSProjectGameInstance::SendPacket(SendBufferRef SendBuffer)
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	GameServerSession->SendPacket(SendBuffer);
}

void UFPSProjectGameInstance::HandleSpawn(const Protocol::ObjectInfo& ObjectInfo, bool IsMine)
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	auto* World = GetWorld();
	if (World == nullptr)
		return;

	// 중복 처리 체크
	const uint64 ObjectId = ObjectInfo.object_id();
	if (Players.Find(ObjectId) != nullptr)
		return;

	FVector SpawnLocation(ObjectInfo.pos_info().x(), ObjectInfo.pos_info().y(), ObjectInfo.pos_info().z());

	// 1. 내 캐릭터인 경우
	if (IsMine)
	{
		auto* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (PC)
		{
			// AFPSBaseCharacter로 캐스팅!
			MyPlayer = Cast<AFPSBaseCharacter>(PC->GetPawn());
			if (MyPlayer)
			{
				MyPlayer->SetPlayerInfo(ObjectInfo.pos_info()); // 내 고유 ID와 위치 정보 세팅
				Players.Add(ObjectId, MyPlayer);               // 맵에 등록
			}
		}
	}
	// 2. 다른 유저의 캐릭터인 경우
	else
	{
		if (OtherPlayerClass == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("OtherPlayerClass is NULL! Please set it in GameInstance Blueprint!"));
			return;
		}

		AFPSBaseCharacter* OtherPlayer = Cast<AFPSBaseCharacter>(World->SpawnActor(OtherPlayerClass, &SpawnLocation));
		if (OtherPlayer)
		{
			OtherPlayer->SetPlayerInfo(ObjectInfo.pos_info()); // 타겟 유저의 ID와 위치 정보 세팅
			Players.Add(ObjectId, OtherPlayer);               // 맵에 등록
		}
	}
}

void UFPSProjectGameInstance::HandleSpawn(const Protocol::S_ENTER_GAME& EnterGamePkt)
{
	HandleSpawn(EnterGamePkt.player(), true);
}

void UFPSProjectGameInstance::HandleSpawn(const Protocol::S_SPAWN& SpawnPkt)
{
	for (auto& Player : SpawnPkt.players())
	{
		HandleSpawn(Player, false);
	}
}

void UFPSProjectGameInstance::HandleDespawn(uint64 ObjectId)
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	auto* World = GetWorld();
	if (World == nullptr)
		return;

	// 1. Players 맵에서 해당 ID를 가진 캐릭터 찾기
	AFPSBaseCharacter** FindActor = Players.Find(ObjectId);
	if (FindActor == nullptr)
		return;

	AFPSBaseCharacter* Player = *FindActor;
	if (Player)
	{
		// 2. 월드에서 캐릭터 제거
		World->DestroyActor(Player);
	}

	// 3. 맵에서 해당 데이터 완전히 삭제 (매우 중요!)
	Players.Remove(ObjectId);
}

void UFPSProjectGameInstance::HandleDespawn(const Protocol::S_DESPAWN& DespawnPkt)
{
	for (auto& ObjectId : DespawnPkt.object_ids())
	{
		HandleDespawn(ObjectId);
	}
}

void UFPSProjectGameInstance::HandleMove(const Protocol::S_MOVE& MovePkt)
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	auto* World = GetWorld();
	if (World == nullptr)
		return;

	const uint64 ObjectId = MovePkt.info().object_id();

	// 1. 패킷이 알려준 ID로 우리 맵에서 캐릭터 찾기
	AFPSBaseCharacter** FindActor = Players.Find(ObjectId);
	if (FindActor == nullptr)
		return;

	AFPSBaseCharacter* Player = (*FindActor);
	if (Player == nullptr)
		return;

	// 2. 내 캐릭터가 서버로부터 내 이동 패킷을 다시 받은 거라면 무시
	if (Player->IsLocallyControlled())
		return;

	// 3. 남의 캐릭터라면 목표 위치(DestInfo)를 갱신
	// 이렇게 갱신해주면 AFPSBaseCharacter::Tick 함수에서 이걸 보고 자연스럽게 걸어갑니다.
	const Protocol::PosInfo& Info = MovePkt.info();
	Player->SetDestInfo(Info);
}

void UFPSProjectGameInstance::Shutdown()
{
	// 게임이 꺼질 때 뒤끝이 없도록 소켓 연결부터 확실히 끊어줍니다.
	DisconnectFromGameServer();

	Super::Shutdown();
}