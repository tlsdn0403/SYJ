#include "FPSProjectGameInstance.h"
#include "FPSNetworkManager.h"
#include "FPSSpawnManager.h"
#include "FPSStage2WorldUtils.h"
#include "FPSStageFlowManager.h"
#include "FPSWorldObjectManager.h"
#include "Weapon/WeaponBase.h"
#include "Protocol.pb.h"
#include "Enum.pb.h"
#include "ClientPacketHandler.h"
#include "Characters/FPSBaseCharacter.h"
#include "Characters/FPSPlayerController.h"
#include "Truck/Truck.h"
#include "Weapon/MountedMachineGun.h"
#include "ADoor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Zombie/BaseZombie.h"
#include "Zombie/ZombieSpawner.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"
#include "HUD/BaseUI.h"
#include "HUD/LoadingUI.h"

UFPSProjectGameInstance::UFPSProjectGameInstance()
{
	NetworkManager = MakeShared<FFPSNetworkManager>();
	SpawnManager = MakeShared<FFPSSpawnManager>(*this);
	StageFlowManager = MakeShared<FFPSStageFlowManager>(*this);

	static ConstructorHelpers::FClassFinder<AFPSBaseCharacter> Character1Class(
		TEXT("/Game/Characters/Blueprint/BP_FPSBaseCharacter"));
	static ConstructorHelpers::FClassFinder<AFPSBaseCharacter> Character2Class(
		TEXT("/Game/Characters/Blueprint/BP_FPSBaseCharacter2"));
	static ConstructorHelpers::FClassFinder<AFPSBaseCharacter> Character3Class(
		TEXT("/Game/Characters/Blueprint/BP_FPSBaseCharacter3"));

	if (Character1Class.Succeeded())
	{
		PlayerCharacterClasses.Add(Character1Class.Class);
		OtherPlayerClass = Character1Class.Class;
	}
	if (Character2Class.Succeeded())
	{
		PlayerCharacterClasses.Add(Character2Class.Class);
	}
	if (Character3Class.Succeeded())
	{
		PlayerCharacterClasses.Add(Character3Class.Class);
	}
}

TSubclassOf<AFPSBaseCharacter> UFPSProjectGameInstance::ResolvePlayerCharacterClass(uint64 ObjectId) const
{
	if (ObjectId > 0 && PlayerCharacterClasses.Num() > 0)
	{
		const int32 CharacterIndex = static_cast<int32>((ObjectId - 1) % PlayerCharacterClasses.Num());
		if (PlayerCharacterClasses.IsValidIndex(CharacterIndex) && PlayerCharacterClasses[CharacterIndex])
		{
			return PlayerCharacterClasses[CharacterIndex];
		}
	}

	return OtherPlayerClass;
}

void UFPSProjectGameInstance::Init()
{
	Super::Init();
	WorldObjects = NewObject<UFPSWorldObjectManager>(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UFPSProjectGameInstance::HandlePostLoadMap);
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

	if (WorldObjects && WorldObjects->DestroyAndRemovePlayer(PlayerId))
	{
		bRemoved = true;
	}

	return bRemoved;
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

void UFPSProjectGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
	MyPlayer = nullptr;
	if (WorldObjects)
	{
		WorldObjects->ClearAll();
	}
	PendingWeaponsByPlayer.Empty();
	PendingStage2SpawnInfos.Reset();
	bProcessingPendingStage2Spawns = false;
	bStage2StartupHoldApplied = false;

	if (StageFlowManager)
	{
		StageFlowManager->HandlePostLoadMap(LoadedWorld);
	}
}

void UFPSProjectGameInstance::RecordStage1CargoItems(const TArray<EItemType>& Items)
{
	for (const EItemType ItemType : Items)
	{
		int32& ItemCount = RecordedStage1CargoItems.FindOrAdd(ItemType);
		++ItemCount;
		UE_LOG(LogTemp, Verbose, TEXT("[Stage1Cargo] Recorded item type=%d total=%d"),
			static_cast<int32>(ItemType),
			ItemCount);
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
	return FPSStage2WorldUtils::IsStage2World(GetWorld());
}

void UFPSProjectGameInstance::RegisterNetworkLootItem(ALootItemBase* LootItem)
{
	if (WorldObjects)
	{
		WorldObjects->RegisterNetworkLootItem(LootItem);
	}
}

void UFPSProjectGameInstance::UnregisterNetworkLootItem(uint64 LootItemId)
{
	if (WorldObjects)
	{
		WorldObjects->UnregisterNetworkLootItem(LootItemId);
	}
}

ALootItemBase* UFPSProjectGameInstance::FindNetworkLootItemById(uint64 LootItemId)
{
	return WorldObjects ? WorldObjects->FindNetworkLootItemById(LootItemId, GetWorld()) : nullptr;
}

bool UFPSProjectGameInstance::TryPickupWeaponLocally(AFPSBaseCharacter* Character, AWeaponBase* Weapon)
{
	if (!ShouldUseLocalInteractionFallback() || Character == nullptr || Weapon == nullptr)
	{
		return false;
	}

	if (WorldObjects)
	{
		WorldObjects->RemoveFieldItem(Weapon->ItemObjectId);
	}
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
		if (IsValid(Truck->GetDriverCharacter()) && Truck->GetDriverCharacter() != Character)
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
	if (SpawnManager)
	{
		SpawnManager->ProcessSpawnObject(ObjectInfo, IsMine);
	}
}

void UFPSProjectGameInstance::HandleSpawn(const Protocol::S_ENTER_GAME& EnterGamePkt)
{
	if (!bProcessingPendingStage2Spawns && ShouldDelayStage2ActorSpawn())
	{
		QueueStage2Spawn(EnterGamePkt.player(), true);
		return;
	}

	if (StageFlowManager)
	{
		StageFlowManager->CompleteStage2MapLoad();
	}
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
	return StageFlowManager && StageFlowManager->ShouldDelayEnterGameRequest();
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

	const bool bLocalCharacterRegistered = IsRegisteredPlayer(LocalCharacter);

	const bool bHoldLocalCharacter =
		bHold ||
		(IsConnectedToGameServer() && LocalCharacter != nullptr && !bLocalCharacterRegistered);
	ApplyCharacterHold(LocalCharacter, bHoldLocalCharacter);

	if (!bHold)
	{
		TArray<TPair<uint64, AFPSBaseCharacter*>> RegisteredPlayers;
		GetValidRegisteredPlayers(RegisteredPlayers);

		for (const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry : RegisteredPlayers)
		{
			AFPSBaseCharacter* RegisteredPlayer = PlayerEntry.Value;
			if (RegisteredPlayer && RegisteredPlayer != LocalCharacter)
			{
				FPSStage2WorldUtils::RestoreNetworkCharacterVisibility(RegisteredPlayer);
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
	if (!IsConnectedToGameServer())
		return;

	if (WorldObjects && WorldObjects->DestroyAndRemoveZombie(ObjectId))
	{
		return;
	}

	if (WorldObjects && WorldObjects->DestroyAndRemoveFieldItem(ObjectId))
	{
		return;
	}

	if (ALootItemBase* LootItem = FindNetworkLootItemById(ObjectId))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[HandleDespawn] Hiding loot item '%s' with NetworkItemId=%llu"),
			*LootItem->GetName(),
			LootItem->GetNetworkItemId());
		LootItem->SetNetworkItemActive(false);
		return;
	}

	// 1. 플레이어 장부에서 해당 ID를 가진 캐릭터 찾기
	RemovePlayerById(ObjectId);
}

void UFPSProjectGameInstance::HandleDespawn(const Protocol::S_DESPAWN& DespawnPkt)
{
	if (!IsConnectedToGameServer())
		return;

	for (const Protocol::DespawnInfo& DespawnInfo : DespawnPkt.despawn_infos())
	{
		const uint64 ObjectId = DespawnInfo.object_id();
		const Protocol::ObjectType ObjectType = DespawnInfo.object_type();

		UE_LOG(LogTemp, Verbose, TEXT("[HandleDespawn] Received ObjectId=%llu Type=%d"), ObjectId, static_cast<int32>(ObjectType));

		switch (ObjectType)
		{
		case Protocol::OBJECT_TYPE_ITEM:
			if (WorldObjects && WorldObjects->DestroyAndRemoveFieldItem(ObjectId))
			{
				continue;
			}

			if (ALootItemBase* LootItem = FindNetworkLootItemById(ObjectId))
			{
				UE_LOG(LogTemp, Verbose, TEXT("[HandleDespawn] Hiding loot item '%s' with NetworkItemId=%llu"),
					*LootItem->GetName(),
					LootItem->GetNetworkItemId());
				LootItem->SetNetworkItemActive(false);
				continue;
			}
			break;

		case Protocol::OBJECT_TYPE_CREATURE:
			if (WorldObjects && WorldObjects->DestroyAndRemoveZombie(ObjectId))
			{
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
	if (!IsConnectedToGameServer())
		return;

	auto* World = GetWorld();
	if (World == nullptr || WorldObjects == nullptr)
		return;

	const uint64 ObjectId = MovePkt.info().object_id();
	if (ObjectId >= 1000000)
	{
		ABaseZombie* Zombie = WorldObjects->FindZombie(ObjectId);
		if (!IsValid(Zombie))
		{
			WorldObjects->RemoveZombie(ObjectId);
			return;
		}

		FVector ZombieLocation(MovePkt.info().x(), MovePkt.info().y(), MovePkt.info().z());
		const UCapsuleComponent* ZombieCapsule = Zombie->GetCapsuleComponent();
		const float ZombieGroundOffset = ZombieCapsule ? ZombieCapsule->GetScaledCapsuleHalfHeight() + 2.0f : 90.0f;
		FPSStage2WorldUtils::TryProjectLocationToGround(World, ZombieLocation, ZombieGroundOffset, ZombieLocation, Zombie);
		const FRotator ZombieRotation(0.0f, MovePkt.info().yaw(), 0.0f);
		const bool bZombieIsMoving = MovePkt.info().state() != Protocol::MOVE_STATE_IDLE;
		Zombie->SetNetworkMoveTarget(ZombieLocation, ZombieRotation, bZombieIsMoving);
		return;
	}

	AFPSBaseCharacter* Player = WorldObjects->FindPlayer(ObjectId);
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

	const bool bIsMountedAimPacket = !FMath::IsNearlyZero(Info.roll());
	if (bIsMountedAimPacket)
	{
		AMountedMachineGun* MountedWeapon = Player->CurrentMountedWeapon;
		if (MountedWeapon == nullptr && IsValid(Player->CurrentTruck))
		{
			MountedWeapon = Player->CurrentTruck->GetMountedWeapon();
			if (MountedWeapon)
			{
				Player->CurrentMountedWeapon = MountedWeapon;
				MountedWeapon->SetWeaponUser(Player);
			}
		}

		if (MountedWeapon)
		{
			MountedWeapon->ApplyNetworkAim(FRotator(Info.pitch(), Info.yaw(), 0.0f));
		}
		return;
	}

	if (Player->IsUsingMountedWeapon() && Player->CurrentMountedWeapon)
	{
		Player->CurrentMountedWeapon->ApplyNetworkAim(
			FRotator(Info.pitch(), Info.yaw(), 0.0f));
		return;
	}

	// 3. 남의 캐릭터라면 목표 위치(DestInfo)를 갱신
	// 이렇게 갱신해주면 AFPSBaseCharacter::Tick 함수에서 이걸 보고 자연스럽게 걸어갑니다.
	Player->SetDestInfo(Info);
}

void UFPSProjectGameInstance::HandleZombieAttack(const Protocol::S_ZOMBIE_ATTACK& pkt)
{
	if (WorldObjects == nullptr)
	{
		return;
	}

	ABaseZombie* Zombie = WorldObjects->FindZombie(pkt.zombie_id());
	if (!IsValid(Zombie))
	{
		WorldObjects->RemoveZombie(pkt.zombie_id());
		return;
	}

	if (Zombie->IsHidden() || !Zombie->GetActorEnableCollision())
	{
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
	if (WorldObjects == nullptr)
	{
		return;
	}

	ABaseZombie* Zombie = WorldObjects->FindZombie(pkt.zombie_id());
	if (!IsValid(Zombie))
	{
		WorldObjects->RemoveZombie(pkt.zombie_id());
		return;
	}

	Zombie->HandleNetworkHit(pkt.hp(), pkt.max_hp());
}

void UFPSProjectGameInstance::HandleZombieDie(const Protocol::S_ZOMBIE_DIE& pkt)
{
	if (WorldObjects == nullptr)
	{
		return;
	}

	ABaseZombie* Zombie = WorldObjects->FindZombie(pkt.zombie_id());
	if (!IsValid(Zombie))
	{
		WorldObjects->RemoveZombie(pkt.zombie_id());
		return;
	}

	Zombie->HandleNetworkDeath();
}

void UFPSProjectGameInstance::HandleZombieDismember(const Protocol::S_ZOMBIE_DISMEMBER& pkt)
{
	if (WorldObjects == nullptr)
	{
		return;
	}

	ABaseZombie* Zombie = WorldObjects->FindZombie(pkt.zombie_id());
	if (!IsValid(Zombie))
	{
		WorldObjects->RemoveZombie(pkt.zombie_id());
		return;
	}

	const FName BoneName(UTF8_TO_TCHAR(pkt.bone_name().c_str()));
	const FVector HitLocation(pkt.hit_x(), pkt.hit_y(), pkt.hit_z());
	const FVector Impulse(pkt.impulse_x(), pkt.impulse_y(), pkt.impulse_z());
	Zombie->HandleNetworkDismember(BoneName, Impulse, HitLocation);
}

ATruck* UFPSProjectGameInstance::FindTruckById(uint64 TruckId)
{
	return WorldObjects
		? WorldObjects->FindTruckById(TruckId, GetWorld(), [](ATruck* Truck)
			{
				FPSStage2WorldUtils::ApplyInitialTruckPlacement(Truck);
			})
		: nullptr;
}

AADoor* UFPSProjectGameInstance::FindDoorById(int32 DoorId)
{
	return WorldObjects ? WorldObjects->FindDoorById(DoorId, GetWorld()) : nullptr;
}

AFPSBaseCharacter* UFPSProjectGameInstance::ResolvePlayerById(uint64 PlayerId) const
{
	return WorldObjects ? WorldObjects->ResolvePlayerById(PlayerId, MyPlayer, GetWorld()) : nullptr;
}

AFPSBaseCharacter* UFPSProjectGameInstance::GetSpectateTargetBySlot(int32 SlotIndex) const
{
	return WorldObjects ? WorldObjects->GetSpectateTargetBySlot(SlotIndex, MyPlayer) : nullptr;
}

bool UFPSProjectGameInstance::IsRegisteredPlayer(AFPSBaseCharacter* Player) const
{
	return WorldObjects ? WorldObjects->ContainsPlayerActor(Player) : false;
}

void UFPSProjectGameInstance::GetValidRegisteredPlayers(TArray<TPair<uint64, AFPSBaseCharacter*>>& OutPlayers) const
{
	if (WorldObjects)
	{
		WorldObjects->GetValidPlayersSorted(OutPlayers);
		return;
	}

	OutPlayers.Reset();
}

void UFPSProjectGameInstance::CacheTruckActors()
{
	if (WorldObjects)
	{
		WorldObjects->CacheTruckActors(GetWorld(), [](ATruck* Truck)
			{
				FPSStage2WorldUtils::ApplyInitialTruckPlacement(Truck);
			});
	}
}

void UFPSProjectGameInstance::CacheDoorActors()
{
	if (WorldObjects)
	{
		WorldObjects->CacheDoorActors(GetWorld());
	}
}

void UFPSProjectGameInstance::HandleEnterTruck(const Protocol::S_ENTER_TRUCK& pkt)
{
	AFPSBaseCharacter* Player = ResolvePlayerById(pkt.player_id());
	AFPSBaseCharacter* MappedPlayer = WorldObjects ? WorldObjects->FindPlayer(pkt.player_id()) : nullptr;
	ATruck* Truck = FindTruckById(pkt.truck_id());
	APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	AFPSBaseCharacter* LocalPawn = LocalPlayerController ? Cast<AFPSBaseCharacter>(LocalPlayerController->GetPawn()) : nullptr;
	const bool bIsLocalPlayer =
		(MyPlayer && MyPlayer->GetPlayerInfo() && MyPlayer->GetPlayerInfo()->object_id() == pkt.player_id()) ||
		(LocalPawn && LocalPawn->GetPlayerInfo() && LocalPawn->GetPlayerInfo()->object_id() == pkt.player_id());
	UE_LOG(LogTemp, Verbose,
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
			UE_LOG(LogTemp, Verbose,
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
	UE_LOG(LogTemp, Verbose,
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

	FVector TargetLocation(pkt.info().x(), pkt.info().y(), pkt.info().z());
	const FRotator TargetRotation(pkt.info().pitch(), pkt.info().yaw(), pkt.info().roll());
	if (pkt.has_truck_fuel() && pkt.fuel() >= 0.0f)
	{
		Truck->SetTruckFuel(pkt.fuel());
	}
	if (pkt.has_truck_health())
	{
		Truck->ApplyNetworkHealth(pkt.truck_hp(), pkt.truck_max_hp());
	}
	Truck->ApplyNetworkTransform(TargetLocation, TargetRotation, pkt.is_correction());
	if (pkt.has_turret_aim())
	{
		if (AMountedMachineGun* MountedWeapon = Truck->GetMountedWeapon())
		{
			MountedWeapon->ApplyNetworkAim(FRotator(pkt.turret_pitch(), pkt.turret_yaw(), 0.0f));
		}
	}
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

void UFPSProjectGameInstance::HandleEquipWeapon(const Protocol::S_EQUIP_WEAPON& pkt)
{
	ApplyEquippedWeapon(pkt.playerid(), pkt.itemobjectid(), pkt.weapontype());
}

void UFPSProjectGameInstance::ApplyEquippedWeapon(uint64 PlayerId, uint64 ItemId, int32 WeaponType)
{
	if (WorldObjects == nullptr)
	{
		return;
	}

	AFPSBaseCharacter* TargetPlayer = WorldObjects->FindPlayer(PlayerId);
	UE_LOG(LogTemp, Verbose, TEXT("[EquipDebug] ApplyEquippedWeapon Start PlayerId=%llu ItemId=%llu WeaponType=%d HasPlayer=%s"),
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

	if (AActor* FieldItemActor = WorldObjects->FindFieldItem(ItemId))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[EquipDebug] Destroy FieldItem Actor ItemId=%llu Actor=%s"),
			ItemId,
			*GetNameSafe(FieldItemActor));
		WorldObjects->DestroyAndRemoveFieldItem(ItemId);
	}

	TSubclassOf<AWeaponBase> WeaponClass = ResolveWeaponClass(WeaponType);
	UE_LOG(LogTemp, Verbose, TEXT("[EquipDebug] ResolveWeaponClass PlayerId=%llu WeaponType=%d Class=%s"),
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
	UE_LOG(LogTemp, Verbose, TEXT("[EquipDebug] Equip Success PlayerId=%llu ItemId=%llu Weapon=%s CurrentWeapon=%s IsLocal=%s"),
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
	if (CurrentWorld == nullptr || WorldObjects == nullptr) return;

	// 패킷에 들어있는 모든 아이템 목록을 순회
	for (int32 i = 0; i < pkt.items_size(); i++)
	{
		const Protocol::ObjectInfo& ItemInfo = pkt.items(i);

		uint64 ItemId = ItemInfo.object_id();
		const Protocol::PosInfo& Pos = ItemInfo.pos_info();

		// 이미 맵(장부)에 소환되어 있는 아이템이면 패스
		if (WorldObjects->FindFieldItem(ItemId))
			continue;

		// 스폰할 좌표 설정
		FVector SpawnLocation(Pos.x(), Pos.y(), Pos.z());
		FRotator SpawnRotation(0.0f, Pos.yaw(), 0.0f);
		FTransform Stage2WeaponSpawnTransform;
		if (FPSStage2WorldUtils::TryGetWeaponSpawnTransform(CurrentWorld, ItemId, SpawnLocation, Stage2WeaponSpawnTransform))
		{
			SpawnLocation = Stage2WeaponSpawnTransform.GetLocation();
			SpawnRotation = Stage2WeaponSpawnTransform.Rotator();
		}

		// 무기 스폰! (에디터에서 DefaultWeaponClass를 지정해뒀어야 함)
		if (DefaultWeaponClass)
		{
			AWeaponBase* SpawnedWeapon = CurrentWorld->SpawnActor<AWeaponBase>(DefaultWeaponClass, SpawnLocation, SpawnRotation);

			if (SpawnedWeapon)
			{
				// 1. 소환된 무기에게 이름표(ID) 달아주기
				SpawnedWeapon->ItemObjectId = ItemId;

				// 2. 바닥 아이템 장부에 등록! (이게 정석의 핵심)
				WorldObjects->RegisterFieldItem(ItemId, SpawnedWeapon);

				UE_LOG(LogTemp, Verbose, TEXT("[Network] %llu번 무기가 맵에 소환되었습니다. WeaponType=%d Location=%s"),
					ItemId,
					ItemInfo.weapon_type(),
					*SpawnLocation.ToString());
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
	if (WorldObjects == nullptr)
	{
		return;
	}

	uint64 ShooterId = pkt.object_id();

	UE_LOG(LogTemp, Verbose, TEXT("[Network] 3. S_FIRE packet received. Shooter=%llu"), ShooterId);

	if (AFPSBaseCharacter* Shooter = WorldObjects->FindPlayer(ShooterId))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[FireDebug] ShooterId=%llu HasPlayer=true Shooter=%s IsLocal=%s HasWeapon=%s Weapon=%s"),
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
	TickNetwork();
	TickStageFlow();
	if (SpawnManager)
	{
		SpawnManager->Tick(DeltaTime);
	}
}

void UFPSProjectGameInstance::TickNetwork()
{
	TrySendEnterGamePacket();
	HandleRecvPackets();
}

TStatId UFPSProjectGameInstance::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFPSProjectGameInstance, STATGROUP_Tickables);
}
