#include "FPSWorldObjectManager.h"
#include "ADoor.h"
#include "Characters/FPSBaseCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Items/LootItemBase.h"
#include "Kismet/GameplayStatics.h"
#include "Truck/Truck.h"
#include "Zombie/BaseZombie.h"
#include "Algo/Sort.h"

void UFPSWorldObjectManager::ClearAll()
{
	Players.Empty();
	Zombies.Empty();
	Trucks.Empty();
	Doors.Empty();
	FieldItems.Empty();
	NetworkLootItems.Empty();
}

void UFPSWorldObjectManager::RegisterPlayer(uint64 PlayerId, AFPSBaseCharacter* Player)
{
	if (PlayerId != 0 && IsValid(Player))
	{
		Players.Add(PlayerId, Player);
	}
}

bool UFPSWorldObjectManager::HasPlayer(uint64 PlayerId) const
{
	return Players.Contains(PlayerId);
}

AFPSBaseCharacter* UFPSWorldObjectManager::FindPlayer(uint64 PlayerId) const
{
	return Players.FindRef(PlayerId);
}

bool UFPSWorldObjectManager::ContainsPlayerActor(const AFPSBaseCharacter* Player) const
{
	if (!IsValid(Player))
	{
		return false;
	}

	for (const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry : Players)
	{
		if (PlayerEntry.Value == Player && IsValid(PlayerEntry.Value))
		{
			return true;
		}
	}

	return false;
}

void UFPSWorldObjectManager::GetValidPlayersSorted(TArray<TPair<uint64, AFPSBaseCharacter*>>& OutPlayers) const
{
	OutPlayers.Reset();
	OutPlayers.Reserve(Players.Num());

	for (const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry : Players)
	{
		if (IsValid(PlayerEntry.Value))
		{
			OutPlayers.Add(PlayerEntry);
		}
	}

	Algo::SortBy(OutPlayers, [](const TPair<uint64, AFPSBaseCharacter*>& PlayerEntry)
	{
		return PlayerEntry.Key;
	});
}

AFPSBaseCharacter* UFPSWorldObjectManager::ResolvePlayerById(uint64 PlayerId, AFPSBaseCharacter* MyPlayer, UWorld* World) const
{
	if (MyPlayer && MyPlayer->GetPlayerInfo() && MyPlayer->GetPlayerInfo()->object_id() == PlayerId)
	{
		return MyPlayer;
	}

	if (World)
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

	return FindPlayer(PlayerId);
}

AFPSBaseCharacter* UFPSWorldObjectManager::GetSpectateTargetBySlot(int32 SlotIndex, AFPSBaseCharacter* MyPlayer) const
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

bool UFPSWorldObjectManager::DestroyAndRemovePlayer(uint64 PlayerId)
{
	if (PlayerId == 0 || !Players.Contains(PlayerId))
	{
		return false;
	}

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

		LeavePlayer->Destroy();
	}

	Players.Remove(PlayerId);
	return true;
}

void UFPSWorldObjectManager::RegisterZombie(uint64 ZombieId, ABaseZombie* Zombie)
{
	if (ZombieId != 0 && IsValid(Zombie))
	{
		Zombies.Add(ZombieId, Zombie);
	}
}

ABaseZombie* UFPSWorldObjectManager::FindZombie(uint64 ZombieId) const
{
	return Zombies.FindRef(ZombieId);
}

void UFPSWorldObjectManager::RemoveZombie(uint64 ZombieId)
{
	Zombies.Remove(ZombieId);
}

bool UFPSWorldObjectManager::DestroyAndRemoveZombie(uint64 ZombieId)
{
	ABaseZombie* Zombie = Zombies.FindRef(ZombieId);
	if (Zombie == nullptr && !Zombies.Contains(ZombieId))
	{
		return false;
	}

	if (IsValid(Zombie))
	{
		Zombie->Destroy();
	}

	Zombies.Remove(ZombieId);
	return true;
}

void UFPSWorldObjectManager::RegisterFieldItem(uint64 ItemId, AActor* ItemActor)
{
	if (ItemId != 0 && IsValid(ItemActor))
	{
		FieldItems.Add(ItemId, ItemActor);
	}
}

AActor* UFPSWorldObjectManager::FindFieldItem(uint64 ItemId) const
{
	return FieldItems.FindRef(ItemId);
}

void UFPSWorldObjectManager::RemoveFieldItem(uint64 ItemId)
{
	FieldItems.Remove(ItemId);
}

bool UFPSWorldObjectManager::DestroyAndRemoveFieldItem(uint64 ItemId)
{
	AActor* FieldItem = FieldItems.FindRef(ItemId);
	if (FieldItem == nullptr && !FieldItems.Contains(ItemId))
	{
		return false;
	}

	if (IsValid(FieldItem))
	{
		FieldItem->Destroy();
	}

	FieldItems.Remove(ItemId);
	return true;
}

void UFPSWorldObjectManager::RegisterNetworkLootItem(ALootItemBase* LootItem)
{
	if (LootItem != nullptr)
	{
		NetworkLootItems.FindOrAdd(LootItem->GetNetworkItemId()) = LootItem;
	}
}

void UFPSWorldObjectManager::UnregisterNetworkLootItem(uint64 LootItemId)
{
	if (LootItemId != 0)
	{
		NetworkLootItems.Remove(LootItemId);
	}
}

ALootItemBase* UFPSWorldObjectManager::FindNetworkLootItemById(uint64 LootItemId, UWorld* World)
{
	if (LootItemId == 0)
	{
		return nullptr;
	}

	if (TObjectPtr<ALootItemBase>* LootItemPtr = NetworkLootItems.Find(LootItemId))
	{
		return LootItemPtr->Get();
	}

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

ATruck* UFPSWorldObjectManager::FindTruckById(uint64 TruckId, UWorld* World, const TFunction<void(ATruck*)>& OnCacheTruck)
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

	CacheTruckActors(World, OnCacheTruck);

	if (ATruck** FoundTruck = Trucks.Find(TruckId))
	{
		return IsValid(*FoundTruck) ? *FoundTruck : nullptr;
	}

	return nullptr;
}

void UFPSWorldObjectManager::CacheTruckActors(UWorld* World, const TFunction<void(ATruck*)>& OnCacheTruck)
{
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
			if (!Truck->IsLocallyDriven() && OnCacheTruck)
			{
				OnCacheTruck(Truck);
			}

			Trucks.Add(Truck->NetworkTruckId, Truck);
		}
	}
}

AADoor* UFPSWorldObjectManager::FindDoorById(int32 DoorId, UWorld* World)
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

	CacheDoorActors(World);

	if (AADoor** FoundDoor = Doors.Find(DoorId))
	{
		return IsValid(*FoundDoor) ? *FoundDoor : nullptr;
	}

	return nullptr;
}

void UFPSWorldObjectManager::CacheDoorActors(UWorld* World)
{
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