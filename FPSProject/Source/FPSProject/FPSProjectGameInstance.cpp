#include "FPSProjectGameInstance.h"
#include "Sockets.h"
#include "Common/TcpSocketBuilder.h"
#include "Serialization/ArrayWriter.h"
#include "SocketSubsystem.h"
#include "PacketSession.h"
#include "Weapon/WeaponBase.h"
#include "Protocol.pb.h"
#include "Enum.pb.h"
#include "ClientPacketHandler.h"
#include "Characters/FPSBaseCharacter.h"
#include "Truck/Truck.h"
#include "Weapon/MountedMachineGun.h"
#include "ADoor.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EngineUtils.h"

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

bool UFPSProjectGameInstance::IsConnectedToGameServer() const
{
	return Socket != nullptr && GameServerSession != nullptr;
}

bool UFPSProjectGameInstance::ShouldUseLocalInteractionFallback() const
{
	return !IsConnectedToGameServer();
}

bool UFPSProjectGameInstance::TryPickupWeaponLocally(AFPSBaseCharacter* Character, AWeaponBase* Weapon)
{
	if (!ShouldUseLocalInteractionFallback() || Character == nullptr || Weapon == nullptr)
	{
		return false;
	}

	FieldItems.Remove(Weapon->ItemObjectId);
	Weapon->SetOwner(Character);
	Weapon->SetInstigator(Character);
	Character->EquipWeapon(Weapon);
	return true;
}

bool UFPSProjectGameInstance::TryEnterTruckLocally(AFPSBaseCharacter* Character, ATruck* Truck, Protocol::TruckSeatType SeatType)
{
	if (!ShouldUseLocalInteractionFallback() || Character == nullptr || Truck == nullptr)
	{
		return false;
	}

	switch (SeatType)
	{
	case Protocol::TRUCK_SEAT_DRIVER:
		if (Truck->GetDriverCharacter() && Truck->GetDriverCharacter() != Character)
		{
			return true;
		}

		Truck->SetLocallyDriven(Character->IsLocallyControlled());
		Truck->SetDriverCharacter(Character);
		Character->EnterTruckDriverSeat(Truck);
		if (AController* PlayerController = Character->GetController())
		{
			PlayerController->Possess(Truck);
			PlayerController->SetControlRotation(Truck->GetActorRotation());
		}
		return true;

	case Protocol::TRUCK_SEAT_CARGO:
		//[신우] 서버 응답을 기다리기 전에도 적재함 탑승 감각이 끊기지 않도록 로컬에서 먼저 태워준다.
		Character->EnterTruckCargo(Truck);
		return true;

	case Protocol::TRUCK_SEAT_TURRET:
		if (AMountedMachineGun* MountedWeapon = Truck->GetMountedWeapon())
		{
			Truck->SetMountedWeaponUser(Character);
			Character->EnterMountedWeapon(Truck, MountedWeapon);
			return true;
		}
		return false;

	default:
		return false;
	}
}

bool UFPSProjectGameInstance::TryExitTruckLocally(AFPSBaseCharacter* Character)
{
	if (!ShouldUseLocalInteractionFallback() || Character == nullptr)
	{
		return false;
	}

	ATruck* Truck = Character->CurrentTruck;

	if (Character->IsDrivingTruck())
	{
		if (Truck == nullptr)
		{
			return true;
		}

		Truck->SetLocallyDriven(false);
		if (Truck->GetDriverCharacter() == Character)
		{
			Truck->SetDriverCharacter(nullptr);
		}

		Character->ExitTruckDriverSeat();
		if (AController* PlayerController = Truck->GetController())
		{
			PlayerController->Possess(Character);
			PlayerController->SetControlRotation(Character->GetActorRotation());
		}
		return true;
	}

	if (Character->IsUsingMountedWeapon())
	{
		//[신우] 기관총에서 F를 누를 때는 "트럭 밖으로 하차"가 아니라 "cargo 좌석으로 복귀"로 해석한다.
		return Truck != nullptr &&
			TryEnterTruckLocally(Character, Truck, Protocol::TRUCK_SEAT_CARGO);
	}

	if (Character->IsOnTruckCargo())
	{
		Character->ExitTruckCargo();
		return true;
	}

	return false;
}

void UFPSProjectGameInstance::HandleSpawn(const Protocol::ObjectInfo& ObjectInfo, bool IsMine)
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	auto* World = GetWorld();
	if (World == nullptr)
		return;

	CacheTruckActors();

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
				UE_LOG(LogTemp, Warning,
					TEXT("[TruckDebug] SpawnMine ObjectId=%llu MyPlayer=%s Local=%d Pawn=%s"),
					ObjectId,
					*GetNameSafe(MyPlayer),
					MyPlayer->IsLocallyControlled() ? 1 : 0,
					*GetNameSafe(PC->GetPawn()));
				RetryPendingWeapon(ObjectId);
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
			UE_LOG(LogTemp, Warning,
				TEXT("[TruckDebug] SpawnOther ObjectId=%llu OtherPlayer=%s Local=%d"),
				ObjectId,
				*GetNameSafe(OtherPlayer),
				OtherPlayer->IsLocallyControlled() ? 1 : 0);
			RetryPendingWeapon(ObjectId);
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

ATruck* UFPSProjectGameInstance::FindTruckById(uint64 TruckId)
{
	if (TruckId == 0)
	{
		return nullptr;
	}

	if (ATruck** FoundTruck = Trucks.Find(TruckId))
	{
		if (IsValid(*FoundTruck))
		{
			return *FoundTruck;
		}

		Trucks.Remove(TruckId);
	}

	CacheTruckActors();

	if (ATruck** FoundTruck = Trucks.Find(TruckId))
	{
		return IsValid(*FoundTruck) ? *FoundTruck : nullptr;
	}

	return nullptr;
}

AADoor* UFPSProjectGameInstance::FindDoorById(int32 DoorId)
{
	if (DoorId == 0)
	{
		return nullptr;
	}

	if (AADoor** FoundDoor = Doors.Find(DoorId))
	{
		if (IsValid(*FoundDoor))
		{
			return *FoundDoor;
		}

		Doors.Remove(DoorId);
	}

	CacheDoorActors();

	if (AADoor** FoundDoor = Doors.Find(DoorId))
	{
		return IsValid(*FoundDoor) ? *FoundDoor : nullptr;
	}

	return nullptr;
}

AFPSBaseCharacter* UFPSProjectGameInstance::ResolvePlayerById(uint64 PlayerId) const
{
	if (MyPlayer && MyPlayer->GetPlayerInfo() && MyPlayer->GetPlayerInfo()->object_id() == PlayerId)
	{
		return MyPlayer;
	}

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (AFPSBaseCharacter* LocalPawn = Cast<AFPSBaseCharacter>(PlayerController->GetPawn()))
			{
				if (LocalPawn->GetPlayerInfo() && LocalPawn->GetPlayerInfo()->object_id() == PlayerId)
				{
					return LocalPawn;
				}
			}
		}
	}

	return Players.FindRef(PlayerId);
}

void UFPSProjectGameInstance::CacheTruckActors()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	Trucks.Empty();

	for (TActorIterator<ATruck> It(World); It; ++It)
	{
		ATruck* Truck = *It;
		if (IsValid(Truck) && Truck->NetworkTruckId != 0)
		{
			Trucks.Add(Truck->NetworkTruckId, Truck);
		}
	}
}

void UFPSProjectGameInstance::CacheDoorActors()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	Doors.Empty();

	for (TActorIterator<AADoor> It(World); It; ++It)
	{
		AADoor* Door = *It;
		if (IsValid(Door) && Door->NetworkDoorId != 0)
		{
			Doors.Add(Door->NetworkDoorId, Door);
		}
	}
}

void UFPSProjectGameInstance::HandleEnterTruck(const Protocol::S_ENTER_TRUCK& pkt)
{
	AFPSBaseCharacter* Player = ResolvePlayerById(pkt.player_id());
	AFPSBaseCharacter* MappedPlayer = Players.FindRef(pkt.player_id());
	ATruck* Truck = FindTruckById(pkt.truck_id());
	APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	AFPSBaseCharacter* LocalPawn = LocalPlayerController ? Cast<AFPSBaseCharacter>(LocalPlayerController->GetPawn()) : nullptr;
	const bool bIsLocalPlayer =
		(MyPlayer && MyPlayer->GetPlayerInfo() && MyPlayer->GetPlayerInfo()->object_id() == pkt.player_id()) ||
		(LocalPawn && LocalPawn->GetPlayerInfo() && LocalPawn->GetPlayerInfo()->object_id() == pkt.player_id());
	UE_LOG(LogTemp, Warning,
		TEXT("[TruckDebug] HandleEnterTruck PlayerId=%llu TruckId=%llu SeatType=%d HasPlayer=%d HasTruck=%d Player=%s Truck=%s MyPlayer=%s MyPlayerId=%llu MappedPlayer=%s MappedPlayerId=%llu LocalPawn=%s LocalPawnId=%llu"),
		pkt.player_id(),
		pkt.truck_id(),
		static_cast<int32>(pkt.seat_type()),
		Player ? 1 : 0,
		Truck ? 1 : 0,
		*GetNameSafe(Player),
		*GetNameSafe(Truck),
		*GetNameSafe(MyPlayer),
		MyPlayer && MyPlayer->GetPlayerInfo() ? MyPlayer->GetPlayerInfo()->object_id() : 0,
		*GetNameSafe(MappedPlayer),
		MappedPlayer && MappedPlayer->GetPlayerInfo() ? MappedPlayer->GetPlayerInfo()->object_id() : 0,
		*GetNameSafe(LocalPawn),
		LocalPawn && LocalPawn->GetPlayerInfo() ? LocalPawn->GetPlayerInfo()->object_id() : 0);

	if (Player == nullptr || Truck == nullptr)
	{
		return;
	}

	switch (pkt.seat_type())
	{
	case Protocol::TRUCK_SEAT_DRIVER:
		Truck->SetLocallyDriven(bIsLocalPlayer);
		Truck->SetDriverCharacter(Player);
		Player->EnterTruckDriverSeat(Truck);
		if (bIsLocalPlayer && LocalPlayerController)
		{
			LocalPlayerController->Possess(Truck);
			LocalPlayerController->SetControlRotation(Truck->GetActorRotation());
			UE_LOG(LogTemp, Warning,
				TEXT("[TruckDebug] EnterDriver Player=%s Truck=%s Controller=%s ViewTarget=%s TruckLoc=%s MeshLoc=%s"),
				*GetNameSafe(Player),
				*GetNameSafe(Truck),
				*GetNameSafe(LocalPlayerController),
				*GetNameSafe(LocalPlayerController->GetViewTarget()),
				*Truck->GetActorLocation().ToString(),
				Truck->GetMesh() ? *Truck->GetMesh()->GetComponentLocation().ToString() : TEXT("NoMesh"));
		}
		break;
	case Protocol::TRUCK_SEAT_CARGO:
		//[신우] 서버가 cargo 탑승을 승인하면 로컬/원격 플레이어 모두 같은 방식으로 적재함 상태를 맞춘다.
		Player->EnterTruckCargo(Truck);
		break;
	case Protocol::TRUCK_SEAT_TURRET:
		if (AMountedMachineGun* MountedWeapon = Truck->GetMountedWeapon())
		{
			//[신우] 서버에서 turret 좌석을 승인한 경우에만 기관총 사용자 정보를 갱신한다.
			Truck->SetMountedWeaponUser(Player);
			Player->EnterMountedWeapon(Truck, MountedWeapon);
		}
		break;
	default:
		break;
	}
}

void UFPSProjectGameInstance::HandleExitTruck(const Protocol::S_EXIT_TRUCK& pkt)
{
	AFPSBaseCharacter* Player = ResolvePlayerById(pkt.player_id());
	ATruck* Truck = FindTruckById(pkt.truck_id());
	UE_LOG(LogTemp, Warning,
		TEXT("[TruckDebug] HandleExitTruck PlayerId=%llu TruckId=%llu SeatType=%d HasPlayer=%d HasTruck=%d Player=%s Truck=%s"),
		pkt.player_id(),
		pkt.truck_id(),
		static_cast<int32>(pkt.seat_type()),
		Player ? 1 : 0,
		Truck ? 1 : 0,
		*GetNameSafe(Player),
		*GetNameSafe(Truck));

	if (Player == nullptr || Truck == nullptr)
	{
		return;
	}

	APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	AFPSBaseCharacter* LocalCharacter = MyPlayer;
	if (LocalCharacter == nullptr && LocalPlayerController)
	{
		LocalCharacter = Cast<AFPSBaseCharacter>(LocalPlayerController->GetCharacter());
	}
	const bool bIsLocalPlayer =
		Player == LocalCharacter ||
		(LocalCharacter && LocalCharacter->GetPlayerInfo() && LocalCharacter->GetPlayerInfo()->object_id() == pkt.player_id());

	switch (pkt.seat_type())
	{
	case Protocol::TRUCK_SEAT_DRIVER:
		Truck->SetLocallyDriven(false);
		if (Truck->GetDriverCharacter() == Player)
		{
			Truck->SetDriverCharacter(nullptr);
		}
		Player->ExitTruckDriverSeat();
		if (bIsLocalPlayer && LocalPlayerController)
		{
			LocalPlayerController->Possess(Player);
			LocalPlayerController->SetControlRotation(Player->GetActorRotation());
		}
		Player->SyncMovementToServer();
		break;
	case Protocol::TRUCK_SEAT_CARGO:
		Player->ExitTruckCargo();
		if (bIsLocalPlayer)
		{
			Player->SyncMovementToServer();
		}
		break;
	case Protocol::TRUCK_SEAT_TURRET:
		if (Truck->GetMountedWeaponUser() == Player)
		{
			Truck->SetMountedWeaponUser(nullptr);
		}
		//[신우] S_EXIT_TRUCK의 turret 케이스는 "기관총에서 cargo로 좌석 전환"이 아니라
		//[신우] "트럭에서 완전히 내린다"는 의미로 사용하고 있어서 false를 넘겨 트럭 밖으로 내보낸다.
		Player->ExitMountedWeapon(false);
		if (bIsLocalPlayer)
		{
			Player->SyncMovementToServer();
		}
		break;
	default:
		break;
	}
}

void UFPSProjectGameInstance::HandleTruckMove(const Protocol::S_TRUCK_MOVE& pkt)
{
	ATruck* Truck = FindTruckById(pkt.info().object_id());
	if (Truck == nullptr)
	{
		return;
	}

	if (Truck->IsLocallyDriven())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[TruckDebug] IgnoreRemoteTruckMove Truck=%s TruckId=%llu"),
			*GetNameSafe(Truck),
			pkt.info().object_id());
		return;
	}

	const FVector TargetLocation(pkt.info().x(), pkt.info().y(), pkt.info().z());
	const FRotator TargetRotation(0.0f, pkt.info().yaw(), 0.0f);
	Truck->SetLocallyDriven(false);
	Truck->SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
	if (USkeletalMeshComponent* TruckMesh = Truck->GetMesh())
	{
		TruckMesh->SetWorldLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void UFPSProjectGameInstance::HandleToggleDoor(const Protocol::S_TOGGLE_DOOR& pkt)
{
	if (AADoor* Door = FindDoorById(pkt.door_id()))
	{
		Door->ApplyDoorState(pkt.is_open());
	}
}

void UFPSProjectGameInstance::HandleEquipWeapon(const Protocol::S_EQUIP_WEAPON& pkt)
{
	ApplyEquippedWeapon(pkt.playerid(), pkt.itemobjectid(), pkt.weapontype());
}

void UFPSProjectGameInstance::ApplyEquippedWeapon(uint64 PlayerId, uint64 ItemId, int32 WeaponType)
{
	AFPSBaseCharacter* TargetPlayer = Players.Contains(PlayerId) ? Players[PlayerId] : nullptr;
	UE_LOG(LogTemp, Warning, TEXT("[EquipDebug] ApplyEquippedWeapon Start PlayerId=%llu ItemId=%llu WeaponType=%d HasPlayer=%s"),
		PlayerId,
		ItemId,
		WeaponType,
		TargetPlayer ? TEXT("true") : TEXT("false"));

	if (TargetPlayer == nullptr)
	{
		FPendingEquippedWeapon& PendingWeapon = PendingWeaponsByPlayer.FindOrAdd(PlayerId);
		PendingWeapon.ItemId = ItemId;
		PendingWeapon.WeaponType = WeaponType;
		UE_LOG(LogTemp, Warning, TEXT("[Network] 플레이어가 아직 없어 무기 장착을 보류합니다. PlayerId=%llu, ItemId=%llu, WeaponType=%d"),
			PlayerId, ItemId, WeaponType);
		return;
	}

	if (FieldItems.Contains(ItemId))
	{
		if (AActor* FieldItemActor = FieldItems[ItemId])
		{
			UE_LOG(LogTemp, Warning, TEXT("[EquipDebug] Destroy FieldItem Actor ItemId=%llu Actor=%s"),
				ItemId,
				*GetNameSafe(FieldItemActor));
			FieldItemActor->Destroy();
		}
		FieldItems.Remove(ItemId);
	}

	TSubclassOf<AWeaponBase> WeaponClass = ResolveWeaponClass(WeaponType);
	UE_LOG(LogTemp, Warning, TEXT("[EquipDebug] ResolveWeaponClass PlayerId=%llu WeaponType=%d Class=%s"),
		PlayerId,
		WeaponType,
		*GetNameSafe(WeaponClass.Get()));
	if (WeaponClass == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[Network] 장착용 무기 클래스가 없어 장착할 수 없습니다. PlayerId=%llu, ItemId=%llu, WeaponType=%d"),
			PlayerId, ItemId, WeaponType);
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = TargetPlayer;
	SpawnParams.Instigator = TargetPlayer;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWeaponBase* EquippedWeapon = World->SpawnActor<AWeaponBase>(WeaponClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (EquippedWeapon == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[Network] 장착용 무기 액터 스폰에 실패했습니다. PlayerId=%llu, ItemId=%llu"), PlayerId, ItemId);
		return;
	}

	EquippedWeapon->ItemObjectId = ItemId;
	TargetPlayer->EquipWeapon(EquippedWeapon);
	UE_LOG(LogTemp, Warning, TEXT("[EquipDebug] Equip Success PlayerId=%llu ItemId=%llu Weapon=%s CurrentWeapon=%s IsLocal=%s"),
		PlayerId,
		ItemId,
		*GetNameSafe(EquippedWeapon),
		*GetNameSafe(TargetPlayer->GetCurrentWeapon()),
		TargetPlayer->IsLocallyControlled() ? TEXT("true") : TEXT("false"));
	PendingWeaponsByPlayer.Remove(PlayerId);
}

void UFPSProjectGameInstance::RetryPendingWeapon(uint64 PlayerId)
{
	const FPendingEquippedWeapon* PendingWeapon = PendingWeaponsByPlayer.Find(PlayerId);
	if (PendingWeapon == nullptr)
	{
		return;
	}

	ApplyEquippedWeapon(PlayerId, PendingWeapon->ItemId, PendingWeapon->WeaponType);
}

TSubclassOf<AWeaponBase> UFPSProjectGameInstance::ResolveWeaponClass(int32 WeaponType) const
{
	switch (WeaponType)
	{
	case Protocol::WEAPON_TYPE_RIFLE:
		return DefaultEquippedWeaponClass ? DefaultEquippedWeaponClass : DefaultWeaponClass;
	default:
		return DefaultEquippedWeaponClass ? DefaultEquippedWeaponClass : DefaultWeaponClass;
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
		UE_LOG(LogTemp, Warning, TEXT("[FireDebug] ShooterId=%llu HasPlayer=true Shooter=%s IsLocal=%s HasWeapon=%s Weapon=%s"),
			ShooterId,
			*GetNameSafe(Shooter),
			(Shooter && Shooter->IsLocallyControlled()) ? TEXT("true") : TEXT("false"),
			(Shooter && Shooter->GetCurrentWeapon()) ? TEXT("true") : TEXT("false"),
			Shooter ? *GetNameSafe(Shooter->GetCurrentWeapon()) : TEXT("null"));

		if (Shooter && !Shooter->IsLocallyControlled() && Shooter->IsUsingMountedWeapon() && Shooter->CurrentMountedWeapon)
		{
			Shooter->CurrentMountedWeapon->SetWeaponUser(Shooter);
			Shooter->CurrentMountedWeapon->Fire();
		}
		else if (Shooter && !Shooter->IsLocallyControlled() && Shooter->GetCurrentWeapon())
		{
			Shooter->GetCurrentWeapon()->RemoteFire();
		}
		else
		{
			// 도착은 했는데 조건에 걸려서 실행이 안 됐을 경우!
			UE_LOG(LogTemp, Error, TEXT("[Network] 에러: 캐릭터를 찾았으나 RemoteFire 조건(무기 장착 등)을 만족하지 못함!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[FireDebug] ShooterId=%llu HasPlayer=false"), ShooterId);
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
