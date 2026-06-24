#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FPSWorldObjectManager.generated.h"

class AADoor;
class AActor;
class ABaseZombie;
class AFPSBaseCharacter;
class ALootItemBase;
class ATruck;

UCLASS()
class FPSPROJECT_API UFPSWorldObjectManager : public UObject
{
	GENERATED_BODY()

public:
	void ClearAll();
	void RegisterPlayer(uint64 PlayerId, AFPSBaseCharacter* Player);
	bool HasPlayer(uint64 PlayerId) const;
	AFPSBaseCharacter* FindPlayer(uint64 PlayerId) const;
	bool ContainsPlayerActor(const AFPSBaseCharacter* Player) const;
	void GetValidPlayersSorted(TArray<TPair<uint64, AFPSBaseCharacter*>>& OutPlayers) const;
	AFPSBaseCharacter* ResolvePlayerById(uint64 PlayerId, AFPSBaseCharacter* MyPlayer, UWorld* World) const;
	AFPSBaseCharacter* GetSpectateTargetBySlot(int32 SlotIndex, AFPSBaseCharacter* MyPlayer) const;
	bool DestroyAndRemovePlayer(uint64 PlayerId);

	void RegisterZombie(uint64 ZombieId, ABaseZombie* Zombie);
	ABaseZombie* FindZombie(uint64 ZombieId) const;
	void RemoveZombie(uint64 ZombieId);
	bool DestroyAndRemoveZombie(uint64 ZombieId);

	void RegisterFieldItem(uint64 ItemId, AActor* ItemActor);
	AActor* FindFieldItem(uint64 ItemId) const;
	void RemoveFieldItem(uint64 ItemId);
	bool DestroyAndRemoveFieldItem(uint64 ItemId);

	void RegisterNetworkLootItem(ALootItemBase* LootItem);
	void UnregisterNetworkLootItem(uint64 LootItemId);
	ALootItemBase* FindNetworkLootItemById(uint64 LootItemId, UWorld* World);

	ATruck* FindTruckById(uint64 TruckId, UWorld* World, const TFunction<void(ATruck*)>& OnCacheTruck);
	void CacheTruckActors(UWorld* World, const TFunction<void(ATruck*)>& OnCacheTruck);
	AADoor* FindDoorById(int32 DoorId, UWorld* World);
	void CacheDoorActors(UWorld* World);

private:
	UPROPERTY()
	TMap<uint64, AFPSBaseCharacter*> Players;

	UPROPERTY()
	TMap<uint64, TObjectPtr<ABaseZombie>> Zombies;

	UPROPERTY()
	TMap<uint64, ATruck*> Trucks;

	UPROPERTY()
	TMap<int32, AADoor*> Doors;

	UPROPERTY()
	TMap<uint64, AActor*> FieldItems;

	UPROPERTY()
	TMap<uint64, TObjectPtr<ALootItemBase>> NetworkLootItems;
};