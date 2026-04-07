// Fill out your copyright notice in the Description page of Project Settings.


#include "FPSProjectGameInstance.h"
#include "Sockets.h"
#include "Common/TcpSocketBuilder.h"
#include "Serialization/ArrayWriter.h"
#include "SocketSubsystem.h"
#include "PacketSession.h"
#include "Weapon/WeaponBase.h"
#include "Protocol.pb.h"
#include "ClientPacketHandler.h"
#include "Characters/FPSBaseCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UFPSProjectGameInstance::ConnectToGameServer(const FString& IPAddress)
{
	FIPv4Address Ip;
	if (FIPv4Address::Parse(IPAddress, Ip) == false)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Connection Failed : 잘못된 IP 주소 형식입니다."));
		return; // 이상한 IP면 여기서 함수를 바로 종료해서 크래시를 막습니다!
	}

	Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(TEXT("Stream"), TEXT("Client Socket"));

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

		//// TEMP : Lobby에서 캐릭터 선택창 등
		//{
		//	Protocol::C_LOGIN Pkt;
		//	SendBufferRef SendBuffer = ClientPacketHandler::MakeSendBuffer(Pkt);
		//	SendPacket(SendBuffer);
		//}
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Connection Failed")));
	}
}

void UFPSProjectGameInstance::DisconnectFromGameServer()
{
	// 서버에 패킷 쏘기
	Protocol::C_LEAVE_GAME LeavePkt;
	SEND_PACKET(LeavePkt);

	// 소켓 통신 닫기
	if (Socket)
	{
		Socket->Close();
		Socket = nullptr;
	}

	// 내 게임 화면 끄기
	UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
}

void UFPSProjectGameInstance::HandleLeaveGame(const Protocol::S_LEAVE_GAME& pkt)
{
	uint64 LeaveId = pkt.object_id();

	// 플레이어 장부(Players)에 나간 사람이 있는지 확인
	if (Players.Contains(LeaveId))
	{
		AFPSBaseCharacter* LeavePlayer = Players[LeaveId];
		if (LeavePlayer)
		{
			// 맵에서 그 캐릭터를 삭제
			LeavePlayer->Destroy();
		}

		// 장부에서도 지워주기
		Players.Remove(LeaveId);
	}
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

void UFPSProjectGameInstance::SendPacketStatic(SendBufferRef SendBuffer)
{
	UWorld* World = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
	if (World == nullptr) World = GWorld;

	if (World)
	{
		if (auto* GI = Cast<UFPSProjectGameInstance>(World->GetGameInstance()))
		{
			GI->SendPacket(SendBuffer);
		}
	}
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
				MyPlayer->SetActorLocation(SpawnLocation);
				Players.Add(ObjectId, MyPlayer);               // 맵에 등록
			}
		}
	}
	// 2. 다른 유저의 캐릭터인 경우
	else
	{
		if (OtherPlayerClass == nullptr) return;

		AFPSBaseCharacter* OtherPlayer = World->SpawnActor<AFPSBaseCharacter>(OtherPlayerClass, SpawnLocation, FRotator::ZeroRotator);
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

void UFPSProjectGameInstance::HandleEquipWeapon(const Protocol::S_EQUIP_WEAPON& pkt)
{
	uint64 PlayerId = pkt.playerid();
	uint64 ItemId = pkt.itemobjectid();

	// 누가 주웠는지 찾기
	AFPSBaseCharacter* TargetPlayer = Players.Contains(PlayerId) ? Players[PlayerId] : nullptr;

	// 바닥에 있는 총 찾기 (FieldItems 맵에 등록해둔 것)
	if (FieldItems.Contains(ItemId))
	{
		AWeaponBase* WeaponActor = Cast<AWeaponBase>(FieldItems[ItemId]);

		if (TargetPlayer && WeaponActor)
		{
			TargetPlayer->EquipWeaponFromField(WeaponActor);

			// 이제 바닥에 없으니 관리 목록에서 제거!
			FieldItems.Remove(ItemId);
		}
	}
}

void UFPSProjectGameInstance::HandleSpawnItem(const Protocol::S_SPAWN_ITEM& pkt)
{
	UWorld* CurrentWorld = GetWorld();
	if (CurrentWorld == nullptr) return;

	// 패킷에 들어있는 모든 아이템 목록을 순회
	for (int32 i = 0; i < pkt.items_size(); i++)
	{
		const Protocol::ObjectInfo& ItemInfo = pkt.items(i);

		uint64 ItemId = ItemInfo.object_id();
		const Protocol::PosInfo& Pos = ItemInfo.pos_info();

		// 이미 맵(장부)에 소환되어 있는 아이템이면 패스
		if (FieldItems.Contains(ItemId))
			continue;

		// 스폰할 좌표 설정
		FVector SpawnLocation(Pos.x(), Pos.y(), Pos.z());

		// 무기 스폰! (에디터에서 DefaultWeaponClass를 지정해뒀어야 함)
		if (DefaultWeaponClass)
		{
			AWeaponBase* SpawnedWeapon = CurrentWorld->SpawnActor<AWeaponBase>(DefaultWeaponClass, SpawnLocation, FRotator::ZeroRotator);

			if (SpawnedWeapon)
			{
				// 1. 소환된 무기에게 이름표(ID) 달아주기
				SpawnedWeapon->ItemObjectId = ItemId;

				// 2. 바닥 아이템 장부에 등록! (이게 정석의 핵심)
				FieldItems.Add(ItemId, SpawnedWeapon);

				UE_LOG(LogTemp, Warning, TEXT("[Network] %llu번 무기가 맵에 소환되었습니다! (위치: %s)"), ItemId, *SpawnLocation.ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Network] DefaultWeaponClass가 세팅되지 않아 무기를 스폰할 수 없습니다!"));
		}
	}
}

void UFPSProjectGameInstance::HandleFire(const Protocol::S_FIRE& pkt)
{
	uint64 ShooterId = pkt.object_id();

	UE_LOG(LogTemp, Error, TEXT("[Network] 3. S_FIRE 패킷 서버로부터 수신 완료! 쏜 사람: %llu"), ShooterId);

	if (Players.Contains(ShooterId))
	{
		AFPSBaseCharacter* Shooter = Players[ShooterId];

		if (Shooter && !Shooter->IsLocallyControlled() && Shooter->GetCurrentWeapon())
		{
			Shooter->GetCurrentWeapon()->RemoteFire();
		}
		else
		{
			// 도착은 했는데 조건에 걸려서 실행이 안 됐을 경우!
			UE_LOG(LogTemp, Error, TEXT("[Network] 에러: 캐릭터를 찾았으나 RemoteFire 조건(무기 장착 등)을 만족하지 못함!"));
		}
	}
}

void UFPSProjectGameInstance::Shutdown()
{
	// 게임이 꺼질 때 뒤끝이 없도록 소켓 연결부터 확실히 끊어줍니다.
	DisconnectFromGameServer();

	Super::Shutdown();
}

void UFPSProjectGameInstance::Tick(float DeltaTime)
{
	HandleRecvPackets();
}

TStatId UFPSProjectGameInstance::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFPSProjectGameInstance, STATGROUP_Tickables);
}

