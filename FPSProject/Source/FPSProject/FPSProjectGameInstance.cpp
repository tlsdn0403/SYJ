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
#include "AIController.h"
#include "Characters/FPSBaseCharacter.h"
#include "Characters/FPSPlayerController.h"
#include "Truck/Truck.h"
#include "Weapon/MountedMachineGun.h"
#include "ADoor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EngineUtils.h"
#include "Zombie/BaseZombie.h"
#include "Zombie/ZombieSpawner.h"
#include "Stage2/Stage2TileManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectGlobals.h"
#include "HUD/BaseUI.h"
#include "HUD/LoadingUI.h"
#include "Items/Stage1ItemSpawnPoint.h"
#include "Algo/Sort.h"

namespace
{
	void RestoreNetworkCharacterVisibility(AFPSBaseCharacter* Character)
	{
		if (!IsValid(Character))
		{
			return;
		}

		Character->SetActorHiddenInGame(false);
		Character->SetActorEnableCollision(true);

		TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
		Character->GetComponents<USkeletalMeshComponent>(SkeletalMeshComponents);
		for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
		{
			if (!IsValid(SkeletalMeshComponent))
			{
				continue;
			}

			SkeletalMeshComponent->SetHiddenInGame(false, true);
			SkeletalMeshComponent->SetVisibility(true, true);
			SkeletalMeshComponent->SetOwnerNoSee(false);
			SkeletalMeshComponent->SetOnlyOwnerSee(false);
		}
	}

	bool IsStage2LevelName(const FString& LevelName)
	{
		return LevelName.Contains(TEXT("map_level2"), ESearchCase::IgnoreCase) ||
			LevelName.Contains(TEXT("level2"), ESearchCase::IgnoreCase) ||
			LevelName.Contains(TEXT("stage2"), ESearchCase::IgnoreCase);
	}

	bool IsStage2World(const UWorld* World)
	{
		return World && IsStage2LevelName(World->GetMapName());
	}

	AStage2TileManager* FindStage2TileManager(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		for (TActorIterator<AStage2TileManager> It(World); It; ++It)
		{
			return *It;
		}

		return nullptr;
	}

	bool TryGetStage2PlayerSpawnTransform(UWorld* World, uint64 ObjectId, FTransform& OutTransform)
	{
		if (!IsStage2World(World))
		{
			return false;
		}

		const AStage2TileManager* Stage2TileManager = FindStage2TileManager(World);
		if (!Stage2TileManager || !Stage2TileManager->AreInitialTilesReady())
		{
			return false;
		}

		FTransform InitialSpawnTransform;
		if (!Stage2TileManager->TryGetInitialPlayerSpawnTransform(InitialSpawnTransform))
		{
			return false;
		}

		static constexpr float PlayerSpawnSpacing = 180.0f;
		const int32 SpawnSlot = static_cast<int32>(ObjectId % 3);
		const float LateralOffset = SpawnSlot == 0 ? 0.0f : (SpawnSlot == 1 ? -PlayerSpawnSpacing : PlayerSpawnSpacing);

		FVector SpawnLocation =
			InitialSpawnTransform.GetLocation() +
			InitialSpawnTransform.GetRotation().GetRightVector() * LateralOffset +
			FVector(0.0f, 0.0f, 180.0f);

		FHitResult GroundHit;
		const FVector TraceStart = SpawnLocation + FVector(0.0f, 0.0f, 500.0f);
		const FVector TraceEnd = SpawnLocation - FVector(0.0f, 0.0f, 2500.0f);
		if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility))
		{
			SpawnLocation = GroundHit.ImpactPoint + FVector(0.0f, 0.0f, 120.0f);
		}

		OutTransform = InitialSpawnTransform;
		OutTransform.SetLocation(SpawnLocation);
		return true;
	}
}

void UFPSProjectGameInstance::Init()
{
	Super::Init();
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UFPSProjectGameInstance::HandlePostLoadMap);
}

void UFPSProjectGameInstance::ConnectToGameServer(const FString& IPAddress)
{
	FIPv4Address Ip;
	if (FIPv4Address::Parse(IPAddress, Ip) == false)
	{
		return; // 이상한 IP면 여기서 함수를 바로 종료해서 크래시를 막습니다!
	}

	Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(TEXT("Stream"), TEXT("Client Socket"));

	TSharedRef<FInternetAddr> InternetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	InternetAddr->SetIp(Ip.Value);
	InternetAddr->SetPort(Port);

	bool Connected = Socket->Connect(*InternetAddr);

	if (Connected)
	{
		// Session
		GameServerSession = MakeShared<PacketSession>(Socket);
		GameServerSession->Run();
	}
	else
	{
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
	RemovePlayerById(LeaveId);
}

bool UFPSProjectGameInstance::RemovePlayerById(uint64 PlayerId)
{
	if (PlayerId == 0)
	{
		return false;
	}

	bool bRemoved = false;

	for (int32 i = PendingStage2SpawnInfos.Num() - 1; i >= 0; --i)
	{
		if (PendingStage2SpawnInfos[i].ObjectInfo.object_id() == PlayerId)
		{
			PendingStage2SpawnInfos.RemoveAtSwap(i);
			bRemoved = true;
		}
	}

	PendingWeaponsByPlayer.Remove(PlayerId);

	// 플레이어 장부(Players)에 나간 사람이 있는지 확인
	if (Players.Contains(PlayerId))
	{
		AFPSBaseCharacter* LeavePlayer = Players[PlayerId];
		if (LeavePlayer)
		{
			for (TPair<uint64, ATruck*>& TruckEntry : Trucks)
			{
				ATruck* Truck = TruckEntry.Value;
				if (Truck == nullptr)
				{
					continue;
				}

				if (Truck->GetDriverCharacter() == LeavePlayer)
				{
					Truck->SetLocallyDriven(false);
					Truck->SetDriverCharacter(nullptr);
				}

				if (Truck->GetMountedWeaponUser() == LeavePlayer)
				{
					Truck->SetMountedWeaponUser(nullptr);
				}
			}

			// 맵에서 그 캐릭터를 삭제
			LeavePlayer->Destroy();
		}

		// 장부에서도 지워주기
		Players.Remove(PlayerId);
		bRemoved = true;
	}

	return bRemoved;
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

bool UFPSProjectGameInstance::SendZombieHitPacket(AFPSBaseCharacter* Attacker, ABaseZombie* Zombie, float Damage, const FVector& HitLocation, FName HitBoneName, const FVector& HitNormal)
{
	if (Attacker == nullptr || Zombie == nullptr || Damage <= 0.0f)
	{
		return false;
	}

	UWorld* World = Attacker->GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	UFPSProjectGameInstance* GameInstance = Cast<UFPSProjectGameInstance>(World->GetGameInstance());
	if (GameInstance == nullptr || !GameInstance->IsConnectedToGameServer())
	{
		return false;
	}

	Protocol::PosInfo* AttackerInfo = Attacker->GetPlayerInfo();
	const uint64 AttackerId = AttackerInfo ? AttackerInfo->object_id() : 0;
	const uint64 ZombieId = Zombie->GetNetworkObjectId();
	if (AttackerId == 0 || ZombieId == 0)
	{
		return false;
	}

	Protocol::C_HIT_ZOMBIE hitPkt;
	hitPkt.set_zombie_id(ZombieId);
	hitPkt.set_attacker_id(AttackerId);
	hitPkt.set_damage(Damage);
	hitPkt.set_hit_x(HitLocation.X);
	hitPkt.set_hit_y(HitLocation.Y);
	hitPkt.set_hit_z(HitLocation.Z);
	if (HitBoneName != NAME_None)
	{
		hitPkt.set_hit_bone_name(TCHAR_TO_UTF8(*HitBoneName.ToString()));
	}
	hitPkt.set_hit_normal_x(HitNormal.X);
	hitPkt.set_hit_normal_y(HitNormal.Y);
	hitPkt.set_hit_normal_z(HitNormal.Z);
	SEND_PACKET(hitPkt);

	return true;
}

bool UFPSProjectGameInstance::IsConnectedToGameServer() const
{
	return Socket != nullptr && GameServerSession != nullptr;
}

bool UFPSProjectGameInstance::ShouldUseLocalInteractionFallback() const
{
	return !IsConnectedToGameServer();
}

bool UFPSProjectGameInstance::ShouldDelayEnterGameRequest() const
{
	if (const AStage2TileManager* Stage2TileManager = FindStage2TileManager(GetWorld()))
	{
		return !Stage2TileManager->AreInitialTilesReady();
	}

	if (bWaitingForStage2MapLoad && IsStage2LevelName(PendingStageTransitionLevelName))
	{
		return true;
	}

	if (IsStage2World(GetWorld()))
	{
		return true;
	}

	return false;
}

void UFPSProjectGameInstance::RequestEnterGameWhenReady()
{
	bPendingEnterGameRequest = true;
	bEnterGamePacketSent = false;
	bShouldShowEntryLoadingWidget = true;
	CachedEntryLoadingReadyCount = 0;
	CachedStage1ItemSpawnSeed = 0;
	bHasStage1ItemSpawnSeed = false;
	bHasAppliedStage1ItemSpawns = false;
	bHasDistributedStage1CargoItems = false;
}

bool UFPSProjectGameInstance::TrySendEnterGamePacket()
{
	if (!bPendingEnterGameRequest || bEnterGamePacketSent)
	{
		return false;
	}

	if (Socket == nullptr || GameServerSession == nullptr)
	{
		return false;
	}

	if (ShouldDelayEnterGameRequest())
	{
		return false;
	}

	Protocol::C_ENTER_GAME EnterGamePkt;
	EnterGamePkt.set_playerindex(0);
	SendBufferRef SendBuffer = ClientPacketHandler::MakeSendBuffer(EnterGamePkt);
	SendPacket(SendBuffer);

	bEnterGamePacketSent = true;
	bPendingEnterGameRequest = false;
	UE_LOG(LogTemp, Warning, TEXT("[Network] Stage2 ready check passed. C_ENTER_GAME 전송 완료!"));
	return true;
}

void UFPSProjectGameInstance::RefreshStage2StartupActorHold()
{
	const bool bShouldHold = ShouldDelayStage2ActorSpawn();
	if (bShouldHold || bStage2StartupHoldApplied)
	{
		ApplyStage2StartupActorHold(bShouldHold);
	}
}

void UFPSProjectGameInstance::SetEntryLoadingWidgetClass(TSubclassOf<UUserWidget> WidgetClass)
{
	EntryLoadingWidgetClass = WidgetClass;
	CachedEntryLoadingReadyCount = 0;
}

void UFPSProjectGameInstance::ShowEntryLoadingWidget()
{
	bShouldShowEntryLoadingWidget = true;

	if (EntryLoadingWidget)
	{
		return;
	}

	if (!EntryLoadingWidgetClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UUserWidget* Widget = CreateWidget<UUserWidget>(World, EntryLoadingWidgetClass);
	if (Widget == nullptr)
	{
		return;
	}

	Widget->AddToViewport();
	RegisterEntryLoadingWidget(Widget);
}

void UFPSProjectGameInstance::RegisterEntryLoadingWidget(UUserWidget* Widget)
{
	EntryLoadingWidget = Widget;
	ApplyEntryLoadingReadyCount(CachedEntryLoadingReadyCount);
}

void UFPSProjectGameInstance::RemoveEntryLoadingWidget()
{
	bShouldShowEntryLoadingWidget = false;

	if (EntryLoadingWidget)
	{
		EntryLoadingWidget->RemoveFromParent();
		EntryLoadingWidget = nullptr;
	}
}

void UFPSProjectGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
	MyPlayer = nullptr;
	Players.Empty();
	Zombies.Empty();
	Trucks.Empty();
	Doors.Empty();
	FieldItems.Empty();
	NetworkLootItems.Empty();
	PendingWeaponsByPlayer.Empty();
	PendingStage2SpawnInfos.Reset();
	bProcessingPendingStage2Spawns = false;
	bStage2StartupHoldApplied = false;
	bHasDistributedStage1CargoItems = false;

	if (EntryLoadingWidget)
	{
		EntryLoadingWidget->RemoveFromParent();
		EntryLoadingWidget = nullptr;
	}

	if (bShouldShowEntryLoadingWidget)
	{
		ShowEntryLoadingWidget();
	}

	bHasAppliedStage1ItemSpawns = false;
	ApplyStage1ItemSpawnSeed();

	if (bWaitingForStage2MapLoad && !IsStage2World(LoadedWorld))
	{
		bWaitingForStage2MapLoad = false;
		PendingStageTransitionLevelName.Empty();
	}
}

void UFPSProjectGameInstance::ApplyEntryLoadingReadyCount(int32 ReadyCount)
{
	CachedEntryLoadingReadyCount = ReadyCount;

	ULoadingUI* LoadingUI = Cast<ULoadingUI>(EntryLoadingWidget);
	if (LoadingUI == nullptr)
	{
		return;
	}

	LoadingUI->logout();
	LoadingUI->OnlineP = FMath::Clamp(ReadyCount, 0, 3);
	LoadingUI->connect(LoadingUI->OnlineP);
}

void UFPSProjectGameInstance::RecordStage1CargoItems(const TArray<EItemType>& Items)
{
	for (const EItemType ItemType : Items)
	{
		RecordedStage1CargoItems.FindOrAdd(ItemType)++;
	}
}

bool UFPSProjectGameInstance::ConsumeRecordedStage1CargoItem(EItemType ItemType, int32 Amount)
{
	if (Amount <= 0)
	{
		return false;
	}

	int32* ItemCount = RecordedStage1CargoItems.Find(ItemType);
	if (ItemCount == nullptr || *ItemCount < Amount)
	{
		return false;
	}

	*ItemCount -= Amount;
	if (*ItemCount <= 0)
	{
		RecordedStage1CargoItems.Remove(ItemType);
	}

	return true;
}

void UFPSProjectGameInstance::ClearRecordedStage1CargoItems()
{
	RecordedStage1CargoItems.Empty();
}

int32 UFPSProjectGameInstance::GetRecordedStage1CargoItemCount(EItemType ItemType) const
{
	if (const int32* ItemCount = RecordedStage1CargoItems.Find(ItemType))
	{
		return *ItemCount;
	}

	return 0;
}

bool UFPSProjectGameInstance::IsInStage2World() const
{
	return IsStage2World(GetWorld());
}

void UFPSProjectGameInstance::RegisterNetworkLootItem(ALootItemBase* LootItem)
{
	if (LootItem == nullptr)
	{
		return;
	}

	NetworkLootItems.FindOrAdd(LootItem->GetNetworkItemId()) = LootItem;
}

void UFPSProjectGameInstance::UnregisterNetworkLootItem(uint64 LootItemId)
{
	if (LootItemId == 0)
	{
		return;
	}

	NetworkLootItems.Remove(LootItemId);
}

ALootItemBase* UFPSProjectGameInstance::FindNetworkLootItemById(uint64 LootItemId)
{
	if (LootItemId == 0)
	{
		return nullptr;
	}

	if (TObjectPtr<ALootItemBase>* LootItemPtr = NetworkLootItems.Find(LootItemId))
	{
		return LootItemPtr->Get();
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<ALootItemBase> It(World); It; ++It)
	{
		ALootItemBase* LootItem = *It;
		if (LootItem == nullptr)
		{
			continue;
		}

		if (LootItem->GetNetworkItemId() == LootItemId)
		{
			RegisterNetworkLootItem(LootItem);
			return LootItem;
		}
	}

	FString KnownItemIds;
	int32 LoggedCount = 0;
	for (const TPair<uint64, TObjectPtr<ALootItemBase>>& Entry : NetworkLootItems)
	{
		if (LoggedCount >= 10)
		{
			KnownItemIds += TEXT(" ...");
			break;
		}

		if (!KnownItemIds.IsEmpty())
		{
			KnownItemIds += TEXT(", ");
		}

		KnownItemIds += FString::Printf(TEXT("%llu"), Entry.Key);
		++LoggedCount;
	}

	UE_LOG(LogTemp, Warning, TEXT("[DespawnLookup] Failed to find LootItemId=%llu. RegisteredIds=[%s]"),
		LootItemId,
		KnownItemIds.IsEmpty() ? TEXT("none") : *KnownItemIds);

	return nullptr;
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
	if (!bProcessingPendingStage2Spawns && ShouldDelayStage2ActorSpawn())
	{
		QueueStage2Spawn(ObjectInfo, IsMine);
		return;
	}

	ProcessSpawnObject(ObjectInfo, IsMine);
}

void UFPSProjectGameInstance::ProcessSpawnObject(const Protocol::ObjectInfo& ObjectInfo, bool IsMine)
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	auto* World = GetWorld();
	if (World == nullptr)
		return;

	CacheTruckActors();

	// 중복 처리 체크
	const uint64 ObjectId = ObjectInfo.object_id();
	if (ObjectId >= 1000000)
	{
		if (Zombies.Find(ObjectId) != nullptr)
		{
			return;
		}

		if (NetworkZombieClass == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] NetworkZombieClass is not assigned."));
			return;
		}

		FVector ZombieLocation(ObjectInfo.pos_info().x(), ObjectInfo.pos_info().y(), ObjectInfo.pos_info().z());
		FRotator ZombieRotation(0.0f, ObjectInfo.pos_info().yaw(), 0.0f);
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ABaseZombie* SpawnedZombie = World->SpawnActor<ABaseZombie>(NetworkZombieClass, ZombieLocation, ZombieRotation, SpawnParams);
		if (SpawnedZombie)
		{
			SpawnedZombie->SetNetworkObjectId(ObjectId);
			SpawnedZombie->SetActorTickEnabled(false);
			if (UCharacterMovementComponent* MoveComp = SpawnedZombie->GetCharacterMovement())
			{
				MoveComp->DisableMovement();
				MoveComp->SetComponentTickEnabled(false);
			}
			if (AAIController* AIController = Cast<AAIController>(SpawnedZombie->GetController()))
			{
				AIController->Destroy();
			}

			Zombies.Add(ObjectId, SpawnedZombie);
		}
		return;
	}

	if (Players.Find(ObjectId) != nullptr)
		return;

	FVector SpawnLocation(ObjectInfo.pos_info().x(), ObjectInfo.pos_info().y(), ObjectInfo.pos_info().z());
	FRotator SpawnRotation(0.0f, ObjectInfo.pos_info().yaw(), 0.0f);
	Protocol::PosInfo SpawnPosInfo;
	SpawnPosInfo.CopyFrom(ObjectInfo.pos_info());

	FTransform Stage2SpawnTransform;
	const bool bUsedStage2SpawnTransform = TryGetStage2PlayerSpawnTransform(World, ObjectId, Stage2SpawnTransform);
	if (bUsedStage2SpawnTransform)
	{
		SpawnLocation = Stage2SpawnTransform.GetLocation();
		SpawnRotation = Stage2SpawnTransform.Rotator();
		SpawnPosInfo.set_x(SpawnLocation.X);
		SpawnPosInfo.set_y(SpawnLocation.Y);
		SpawnPosInfo.set_z(SpawnLocation.Z);
		SpawnPosInfo.set_yaw(SpawnRotation.Yaw);
		UE_LOG(LogTemp, Warning, TEXT("[Stage2Spawn] Override player spawn. ObjectId=%llu Location=%s"),
			ObjectId,
			*SpawnLocation.ToString());
	}

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
				MyPlayer->SetPlayerInfo(SpawnPosInfo);
				MyPlayer->SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
				MyPlayer->SetActorHiddenInGame(false);
				MyPlayer->SetActorEnableCollision(true);
				if (UCharacterMovementComponent* MoveComp = MyPlayer->GetCharacterMovement())
				{
					MoveComp->StopMovementImmediately();
					MoveComp->SetMovementMode(MOVE_Walking);
				}
				Players.Add(ObjectId, MyPlayer);
				RetryPendingWeapon(ObjectId);
				ApplyStageTimerToLocalUI();
				if (bUsedStage2SpawnTransform)
				{
					MyPlayer->SyncMovementToServer();
				}

				UE_LOG(LogTemp, Warning,
					TEXT("[TruckDebug] SpawnMine ObjectId=%llu MyPlayer=%s Local=%d Pawn=%s"),
					ObjectId,
					*GetNameSafe(MyPlayer),
					MyPlayer->IsLocallyControlled() ? 1 : 0,
					*GetNameSafe(PC->GetPawn()));
				UE_LOG(LogTemp, Warning, TEXT("[ZombieSync] My player spawned. PlayerId=%llu"), ObjectId);
			}
		}
	}
	// 2. 다른 유저의 캐릭터인 경우
	else
	{
		if (OtherPlayerClass == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("[Stage2Spawn] OtherPlayerClass is null. ObjectId=%llu"), ObjectId);
			return;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AFPSBaseCharacter* OtherPlayer = World->SpawnActor<AFPSBaseCharacter>(OtherPlayerClass, SpawnLocation, SpawnRotation, SpawnParams);
		if (OtherPlayer)
		{
			OtherPlayer->SetPlayerInfo(SpawnPosInfo); // 타겟 유저의 ID와 위치 정보 세팅
			OtherPlayer->SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
			RestoreNetworkCharacterVisibility(OtherPlayer);
			if (UCharacterMovementComponent* MoveComp = OtherPlayer->GetCharacterMovement())
			{
				MoveComp->StopMovementImmediately();
				MoveComp->SetMovementMode(MOVE_Walking);
			}
			Players.Add(ObjectId, OtherPlayer);               // 맵에 등록
			UE_LOG(LogTemp, Warning,
				TEXT("[TruckDebug] SpawnOther ObjectId=%llu OtherPlayer=%s Local=%d Location=%s Hidden=%d"),
				ObjectId,
				*GetNameSafe(OtherPlayer),
				OtherPlayer->IsLocallyControlled() ? 1 : 0,
				*OtherPlayer->GetActorLocation().ToString(),
				OtherPlayer->IsHidden() ? 1 : 0);
			RetryPendingWeapon(ObjectId);
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("[Stage2Spawn] Failed to spawn other player. ObjectId=%llu Class=%s Location=%s"),
				ObjectId,
				*GetNameSafe(OtherPlayerClass.Get()),
				*SpawnLocation.ToString());
		}
	}
}

void UFPSProjectGameInstance::HandleSpawn(const Protocol::S_ENTER_GAME& EnterGamePkt)
{
	if (!bProcessingPendingStage2Spawns && ShouldDelayStage2ActorSpawn())
	{
		QueueStage2Spawn(EnterGamePkt.player(), true);
		return;
	}

	RemoveEntryLoadingWidget();
	bWaitingForStage2MapLoad = false;
	PendingStageTransitionLevelName.Empty();
	HandleSpawn(EnterGamePkt.player(), true);
}

void UFPSProjectGameInstance::HandleSpawn(const Protocol::S_SPAWN& SpawnPkt)
{
	for (auto& Player : SpawnPkt.players())
	{
		HandleSpawn(Player, false);
	}
}

bool UFPSProjectGameInstance::ShouldDelayStage2ActorSpawn() const
{
	if (const AStage2TileManager* Stage2TileManager = FindStage2TileManager(GetWorld()))
	{
		return !Stage2TileManager->AreInitialTilesReady();
	}

	if (bWaitingForStage2MapLoad && IsStage2LevelName(PendingStageTransitionLevelName))
	{
		return true;
	}

	if (IsStage2World(GetWorld()))
	{
		return true;
	}

	return false;
}

void UFPSProjectGameInstance::QueueStage2Spawn(const Protocol::ObjectInfo& ObjectInfo, bool IsMine)
{
	const uint64 ObjectId = ObjectInfo.object_id();
	for (FPendingStage2SpawnInfo& PendingSpawn : PendingStage2SpawnInfos)
	{
		if (PendingSpawn.ObjectInfo.object_id() == ObjectId)
		{
			PendingSpawn.ObjectInfo = ObjectInfo;
			PendingSpawn.bIsMine = PendingSpawn.bIsMine || IsMine;
			return;
		}
	}

	FPendingStage2SpawnInfo& PendingSpawn = PendingStage2SpawnInfos.AddDefaulted_GetRef();
	PendingSpawn.ObjectInfo = ObjectInfo;
	PendingSpawn.bIsMine = IsMine;
}

void UFPSProjectGameInstance::ProcessPendingStage2Spawns()
{
	if (PendingStage2SpawnInfos.Num() == 0 || ShouldDelayStage2ActorSpawn())
	{
		return;
	}

	TArray<FPendingStage2SpawnInfo> SpawnsToProcess = MoveTemp(PendingStage2SpawnInfos);
	PendingStage2SpawnInfos.Reset();

	TGuardValue<bool> ProcessingGuard(bProcessingPendingStage2Spawns, true);
	for (const FPendingStage2SpawnInfo& PendingSpawn : SpawnsToProcess)
	{
		if (PendingSpawn.bIsMine)
		{
			RemoveEntryLoadingWidget();
			bWaitingForStage2MapLoad = false;
			PendingStageTransitionLevelName.Empty();
		}

		ProcessSpawnObject(PendingSpawn.ObjectInfo, PendingSpawn.bIsMine);
	}

	TryDistributeStage1CargoItemsToPlayers();
}

void UFPSProjectGameInstance::TryDistributeStage1CargoItemsToPlayers()
{
	if (bHasDistributedStage1CargoItems || RecordedStage1CargoItems.Num() == 0)
	{
		return;
	}

	if (!IsStage2World(GetWorld()) || ShouldDelayStage2ActorSpawn() || PendingStage2SpawnInfos.Num() > 0)
	{
		return;
	}

	if (IsConnectedToGameServer() && CachedEntryLoadingReadyCount <= 0)
	{
		return;
	}

	TArray<TPair<uint64, AFPSBaseCharacter*>> Stage2Players;
	Stage2Players.Reserve(Players.Num());
	for (const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry : Players)
	{
		if (IsValid(PlayerEntry.Value))
		{
			Stage2Players.Add(PlayerEntry);
		}
	}

	Algo::SortBy(Stage2Players, [](const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry)
	{
		return PlayerEntry.Key;
	});

	const int32 ExpectedPlayerCount = FMath::Max(CachedEntryLoadingReadyCount, 1);
	if (Stage2Players.Num() < ExpectedPlayerCount)
	{
		return;
	}

	const int32 PlayerCount = Stage2Players.Num();
	for (const TPair<EItemType, int32>& CargoEntry : RecordedStage1CargoItems)
	{
		const EItemType ItemType = CargoEntry.Key;
		const int32 ItemCount = CargoEntry.Value;
		if (ItemType == EItemType::None || ItemCount <= 0)
		{
			continue;
		}

		const int32 BaseShare = ItemCount / PlayerCount;
		const int32 Remainder = ItemCount % PlayerCount;

		for (const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry : Stage2Players)
		{
			for (int32 i = 0; i < BaseShare; ++i)
			{
				PlayerEntry.Value->AddStage2DistributedItem(ItemType);
			}
		}

		if (Remainder > 0)
		{
			TArray<int32> RemainderPlayerIndexes;
			RemainderPlayerIndexes.Reserve(PlayerCount);
			for (int32 PlayerIndex = 0; PlayerIndex < PlayerCount; ++PlayerIndex)
			{
				RemainderPlayerIndexes.Add(PlayerIndex);
			}

			uint32 RemainderSeed = static_cast<uint32>(ItemType) * 16777619u ^ static_cast<uint32>(ItemCount);
			for (const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry : Stage2Players)
			{
				RemainderSeed ^= static_cast<uint32>(PlayerEntry.Key);
				RemainderSeed *= 16777619u;
				RemainderSeed ^= static_cast<uint32>(PlayerEntry.Key >> 32);
			}

			FRandomStream RandomStream(static_cast<int32>(RemainderSeed));
			for (int32 i = RemainderPlayerIndexes.Num() - 1; i > 0; --i)
			{
				const int32 SwapIndex = RandomStream.RandRange(0, i);
				RemainderPlayerIndexes.Swap(i, SwapIndex);
			}

			for (int32 i = 0; i < Remainder; ++i)
			{
				Stage2Players[RemainderPlayerIndexes[i]].Value->AddStage2DistributedItem(ItemType);
			}
		}
	}

	for (const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry : Stage2Players)
	{
		PlayerEntry.Value->RefreshStage2ItemUI();
	}

	ClearRecordedStage1CargoItems();
	bHasDistributedStage1CargoItems = true;
	UE_LOG(LogTemp, Log, TEXT("[Stage2Cargo] Distributed Stage1 cargo to %d players."), PlayerCount);
}

void UFPSProjectGameInstance::ApplyStage2StartupActorHold(bool bHold)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	auto ApplyCharacterHold = [this](AFPSBaseCharacter* Character, bool bShouldHold)
		{
			if (!IsValid(Character))
			{
				return;
			}

			Character->SetActorHiddenInGame(bShouldHold);
			Character->SetActorEnableCollision(!bShouldHold);

			if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
			{
				Capsule->SetCollisionEnabled(bShouldHold ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
			}

			if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
			{
				if (bShouldHold)
				{
					MoveComp->StopMovementImmediately();
					MoveComp->DisableMovement();
				}
				else
				{
					MoveComp->SetMovementMode(MOVE_Walking);
				}
			}
		};

	AFPSBaseCharacter* LocalCharacter = nullptr;
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0))
	{
		LocalCharacter = Cast<AFPSBaseCharacter>(PlayerController->GetPawn());
	}

	bool bLocalCharacterRegistered = false;
	if (LocalCharacter)
	{
		for (const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry : Players)
		{
			if (PlayerEntry.Value == LocalCharacter)
			{
				bLocalCharacterRegistered = true;
				break;
			}
		}
	}

	const bool bHoldLocalCharacter =
		bHold ||
		(IsConnectedToGameServer() && LocalCharacter != nullptr && !bLocalCharacterRegistered);
	ApplyCharacterHold(LocalCharacter, bHoldLocalCharacter);

	if (!bHold)
	{
		for (const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry : Players)
		{
			AFPSBaseCharacter* RegisteredPlayer = PlayerEntry.Value;
			if (RegisteredPlayer && RegisteredPlayer != LocalCharacter)
			{
				RestoreNetworkCharacterVisibility(RegisteredPlayer);
			}
		}
	}

	for (TActorIterator<ATruck> It(World); It; ++It)
	{
		ATruck* Truck = *It;
		if (!IsValid(Truck))
		{
			continue;
		}

		Truck->SetActorHiddenInGame(bHold);
		Truck->SetActorEnableCollision(!bHold);

		if (USkeletalMeshComponent* TruckMesh = Truck->GetMesh())
		{
			if (!IsValid(TruckMesh) || !TruckMesh->IsRegistered())
			{
				continue;
			}

			TruckMesh->SetEnableGravity(!bHold);
			if (bHold)
			{
				TruckMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
				TruckMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				TruckMesh->PutAllRigidBodiesToSleep();
			}
			else
			{
				TruckMesh->WakeAllRigidBodies();
			}
		}
	}

	bStage2StartupHoldApplied = bHold;
}

void UFPSProjectGameInstance::HandleDespawn(uint64 ObjectId)
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	auto* World = GetWorld();
	if (World == nullptr)
		return;

	if (ABaseZombie* Zombie = Zombies.FindRef(ObjectId))
	{
		if (IsValid(Zombie))
		{
			World->DestroyActor(Zombie);
		}
		Zombies.Remove(ObjectId);
		return;
	}

	if (AActor** ItemActor = FieldItems.Find(ObjectId))
	{
		if (AActor* Actor = *ItemActor)
		{
			World->DestroyActor(Actor);
		}

		FieldItems.Remove(ObjectId);
		return;
	}

	if (ALootItemBase* LootItem = FindNetworkLootItemById(ObjectId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[HandleDespawn] Hiding loot item '%s' with NetworkItemId=%llu"),
			*LootItem->GetName(),
			LootItem->GetNetworkItemId());
		LootItem->SetNetworkItemActive(false);
		return;
	}

	// 1. Players 맵에서 해당 ID를 가진 캐릭터 찾기
	RemovePlayerById(ObjectId);
}

void UFPSProjectGameInstance::HandleDespawn(const Protocol::S_DESPAWN& DespawnPkt)
{
	if (Socket == nullptr || GameServerSession == nullptr)
		return;

	auto* World = GetWorld();
	if (World == nullptr)
		return;

	for (const Protocol::DespawnInfo& DespawnInfo : DespawnPkt.despawn_infos())
	{
		const uint64 ObjectId = DespawnInfo.object_id();
		const Protocol::ObjectType ObjectType = DespawnInfo.object_type();

		UE_LOG(LogTemp, Warning, TEXT("[HandleDespawn] Received ObjectId=%llu Type=%d"), ObjectId, static_cast<int32>(ObjectType));

		switch (ObjectType)
		{
		case Protocol::OBJECT_TYPE_ITEM:
			if (AActor** ItemActor = FieldItems.Find(ObjectId))
			{
				if (AActor* Actor = *ItemActor)
				{
					World->DestroyActor(Actor);
				}

				FieldItems.Remove(ObjectId);
				continue;
			}

			if (ALootItemBase* LootItem = FindNetworkLootItemById(ObjectId))
			{
				UE_LOG(LogTemp, Warning, TEXT("[HandleDespawn] Hiding loot item '%s' with NetworkItemId=%llu"),
					*LootItem->GetName(),
					LootItem->GetNetworkItemId());
				LootItem->SetNetworkItemActive(false);
				continue;
			}
			break;

		case Protocol::OBJECT_TYPE_CREATURE:
			if (ABaseZombie* Zombie = Zombies.FindRef(ObjectId))
			{
				if (IsValid(Zombie))
				{
					World->DestroyActor(Zombie);
				}
				Zombies.Remove(ObjectId);
				continue;
			}

			if (RemovePlayerById(ObjectId))
			{
				continue;
			}
			break;

		default:
			break;
		}

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
	if (ObjectId >= 1000000)
	{
		ABaseZombie* Zombie = Zombies.FindRef(ObjectId);
		if (!IsValid(Zombie))
		{
			Zombies.Remove(ObjectId);
			return;
		}

		const FVector ZombieLocation(MovePkt.info().x(), MovePkt.info().y(), MovePkt.info().z());
		const FRotator ZombieRotation(0.0f, MovePkt.info().yaw(), 0.0f);
		const bool bZombieIsMoving = MovePkt.info().state() != Protocol::MOVE_STATE_IDLE;
		Zombie->SetNetworkMoveTarget(ZombieLocation, ZombieRotation, bZombieIsMoving);
		return;
	}

	// 1. 패킷이 알려준 ID로 우리 맵에서 캐릭터 찾기
	AFPSBaseCharacter** FindActor = Players.Find(ObjectId);
	if (FindActor == nullptr)
		return;

	AFPSBaseCharacter* Player = (*FindActor);
	if (Player == nullptr)
		return;

	const Protocol::PosInfo& Info = MovePkt.info();
	if (Info.state() == Protocol::MOVE_STATE_DEAD)
	{
		Player->SetDestInfo(Info);
		Player->Die(false);
		return;
	}

	// 2. 내 캐릭터가 서버로부터 내 이동 패킷을 다시 받은 거라면 무시
	if (Player->IsLocallyControlled())
		return;

	// 3. 남의 캐릭터라면 목표 위치(DestInfo)를 갱신
	// 이렇게 갱신해주면 AFPSBaseCharacter::Tick 함수에서 이걸 보고 자연스럽게 걸어갑니다.
	Player->SetDestInfo(Info);
}

void UFPSProjectGameInstance::HandleZombieAttack(const Protocol::S_ZOMBIE_ATTACK& pkt)
{
	ABaseZombie* Zombie = Zombies.FindRef(pkt.zombie_id());
	if (!IsValid(Zombie))
	{
		Zombies.Remove(pkt.zombie_id());
		return;
	}

	AFPSBaseCharacter* TargetPlayer = ResolvePlayerById(pkt.target_player_id());
	AActor* TargetActor = TargetPlayer;
	if (TargetPlayer &&
		IsValid(TargetPlayer->CurrentTruck) &&
		(TargetPlayer->IsDrivingTruck() || TargetPlayer->IsOnTruckCargo() || TargetPlayer->IsUsingMountedWeapon()))
	{
		TargetActor = TargetPlayer->CurrentTruck;
	}

	const bool bTargetIsLocalPlayer =
		TargetPlayer && (TargetPlayer == MyPlayer || TargetPlayer->IsLocallyControlled());
	const bool bShouldApplyDamage = TargetActor && (TargetActor->IsA<ATruck>() || bTargetIsLocalPlayer);
	Zombie->HandleNetworkAttack(TargetActor, bShouldApplyDamage);
}

void UFPSProjectGameInstance::HandleZombieHp(const Protocol::S_ZOMBIE_HP& pkt)
{
	ABaseZombie* Zombie = Zombies.FindRef(pkt.zombie_id());
	if (!IsValid(Zombie))
	{
		Zombies.Remove(pkt.zombie_id());
		return;
	}

	Zombie->HandleNetworkHit(pkt.hp(), pkt.max_hp());
}

void UFPSProjectGameInstance::HandleZombieDie(const Protocol::S_ZOMBIE_DIE& pkt)
{
	ABaseZombie* Zombie = Zombies.FindRef(pkt.zombie_id());
	if (!IsValid(Zombie))
	{
		Zombies.Remove(pkt.zombie_id());
		return;
	}

	Zombie->HandleNetworkDeath();
}

void UFPSProjectGameInstance::HandleZombieDismember(const Protocol::S_ZOMBIE_DISMEMBER& pkt)
{
	ABaseZombie* Zombie = Zombies.FindRef(pkt.zombie_id());
	if (!IsValid(Zombie))
	{
		Zombies.Remove(pkt.zombie_id());
		return;
	}

	const FName BoneName(UTF8_TO_TCHAR(pkt.bone_name().c_str()));
	const FVector HitLocation(pkt.hit_x(), pkt.hit_y(), pkt.hit_z());
	const FVector Impulse(pkt.impulse_x(), pkt.impulse_y(), pkt.impulse_z());
	Zombie->HandleNetworkDismember(BoneName, Impulse, HitLocation);
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

AFPSBaseCharacter* UFPSProjectGameInstance::GetSpectateTargetBySlot(int32 SlotIndex) const
{
	if (SlotIndex < 0)
	{
		return nullptr;
	}

	TArray<TPair<uint64, AFPSBaseCharacter*>> SpectateCandidates;
	SpectateCandidates.Reserve(Players.Num());

	for (const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry : Players)
	{
		AFPSBaseCharacter* Player = PlayerEntry.Value;
		if (!IsValid(Player) || Player == MyPlayer || Player->IsDead())
		{
			continue;
		}

		SpectateCandidates.Add(PlayerEntry);
	}

	Algo::SortBy(SpectateCandidates, [](const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry)
	{
		return PlayerEntry.Key;
	});

	return SpectateCandidates.IsValidIndex(SlotIndex) ? SpectateCandidates[SlotIndex].Value : nullptr;
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

	const FVector TargetLocation(pkt.info().x(), pkt.info().y(), pkt.info().z());
	const FRotator TargetRotation(pkt.info().pitch(), pkt.info().yaw(), pkt.info().roll());
	Truck->ApplyNetworkTransform(TargetLocation, TargetRotation, pkt.is_correction());
}

void UFPSProjectGameInstance::HandleLoadTruckItem(const Protocol::S_LOAD_TRUCK_ITEM& pkt)
{
	const uint64 LocalPlayerId = (MyPlayer && MyPlayer->GetPlayerInfo()) ? MyPlayer->GetPlayerInfo()->object_id() : 0;
	if (LocalPlayerId != 0 && pkt.player_id() == LocalPlayerId)
	{
		return;
	}

	ATruck* Truck = FindTruckById(pkt.truck_id());
	if (Truck == nullptr)
	{
		return;
	}

	TArray<EItemType> LoadedItems;
	LoadedItems.Reserve(pkt.item_types_size());

	for (const int32 ItemTypeValue : pkt.item_types())
	{
		const EItemType ItemType = static_cast<EItemType>(ItemTypeValue);
		LoadedItems.Add(ItemType);
		Truck->ApplyLoadedCargoItem(ItemType);
	}

	RecordStage1CargoItems(LoadedItems);
}

void UFPSProjectGameInstance::HandleToggleDoor(const Protocol::S_TOGGLE_DOOR& pkt)
{
	if (AADoor* Door = FindDoorById(pkt.door_id()))
	{
		Door->ApplyDoorState(pkt.is_open());
	}
}

void UFPSProjectGameInstance::HandleEnterGameReadyCount(const Protocol::S_ENTER_GAME_READY_COUNT& pkt)
{
	ApplyEntryLoadingReadyCount(pkt.ready_count());
}

void UFPSProjectGameInstance::HandleStageTimer(const Protocol::S_STAGE_TIMER& pkt)
{
	CachedStageTimerRemainingSeconds = pkt.is_loading_phase() ? pkt.remaining_seconds() : 0;
	ApplyStageTimerToLocalUI();
}

void UFPSProjectGameInstance::HandleStage1ItemSeed(const Protocol::S_STAGE1_ITEM_SEED& pkt)
{
	CachedStage1ItemSpawnSeed = pkt.seed();
	bHasStage1ItemSpawnSeed = true;
	bHasAppliedStage1ItemSpawns = false;
	ApplyStage1ItemSpawnSeed();
}

void UFPSProjectGameInstance::HandleRespawnLootItem(const Protocol::S_RESPAWN_LOOT_ITEM& pkt)
{
	for (uint64 ItemId : pkt.item_object_ids())
	{
		if (ALootItemBase* LootItem = FindNetworkLootItemById(ItemId))
		{
			LootItem->SetNetworkItemActive(true);
		}
	}
}

void UFPSProjectGameInstance::HandleStageTransition(const Protocol::S_STAGE_TRANSITION& pkt)
{
	const FString TargetLevelName = UTF8_TO_TCHAR(pkt.target_level().c_str());
	if (TargetLevelName.IsEmpty())
	{
		return;
	}

	PendingStageTransitionLevelName = TargetLevelName;
	bWaitingForStage2MapLoad = IsStage2LevelName(TargetLevelName);
	UGameplayStatics::OpenLevel(this, FName(*TargetLevelName));
}

void UFPSProjectGameInstance::ApplyStageTimerToLocalUI()
{
	if (CachedStageTimerRemainingSeconds == INDEX_NONE)
		return;

	AFPSPlayerController* PlayerController = Cast<AFPSPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (PlayerController == nullptr || PlayerController->TimerW == nullptr)
		return;

	PlayerController->TimerW->SetRemainingTime(CachedStageTimerRemainingSeconds);
}

void UFPSProjectGameInstance::ApplyStage1ItemSpawnSeed()
{
	if (!bHasStage1ItemSpawnSeed || bHasAppliedStage1ItemSpawns)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	TArray<AStage1ItemSpawnPoint*> SpawnPoints;
	for (TActorIterator<AStage1ItemSpawnPoint> It(World); It; ++It)
	{
		if (AStage1ItemSpawnPoint* SpawnPoint = *It)
		{
			SpawnPoints.Add(SpawnPoint);
		}
	}

	Algo::SortBy(SpawnPoints, [](const AStage1ItemSpawnPoint* SpawnPoint)
	{
		return GetPathNameSafe(SpawnPoint);
	});

	FRandomStream RandomStream(static_cast<int32>(CachedStage1ItemSpawnSeed));
	for (AStage1ItemSpawnPoint* SpawnPoint : SpawnPoints)
	{
		if (SpawnPoint == nullptr)
		{
			continue;
		}

		SpawnPoint->ClearSpawnedItem();
		SpawnPoint->SpawnItemFromRandomStream(RandomStream);
		if (ALootItemBase* LootItem = SpawnPoint->GetSpawnedItem())
		{
			RegisterNetworkLootItem(LootItem);
		}
	}

	bHasAppliedStage1ItemSpawns = true;
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
		else if (Shooter && !Shooter->IsLocallyControlled() && IsValid(Shooter->GetCurrentWeapon()))
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
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	RemoveEntryLoadingWidget();
	// 게임이 꺼질 때 뒤끝이 없도록 소켓 연결부터 확실히 끊어줍니다.
	DisconnectFromGameServer();

	Super::Shutdown();
}

void UFPSProjectGameInstance::Tick(float DeltaTime)
{
	RefreshStage2StartupActorHold();
	TrySendEnterGamePacket();
	HandleRecvPackets();
	ProcessPendingStage2Spawns();
	TryDistributeStage1CargoItemsToPlayers();
	RefreshStage2StartupActorHold();
}

TStatId UFPSProjectGameInstance::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFPSProjectGameInstance, STATGROUP_Tickables);
}