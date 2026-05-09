#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Tickable.h"
#include "FPSProject.h"
#include "Enum.pb.h"
#include "Items/LootItemBase.h"
#include "FPSProjectGameInstance.generated.h"

/**
 * 
 */

UCLASS()
class FPSPROJECT_API UFPSProjectGameInstance : public UGameInstance, public FTickableGameObject
{
	GENERATED_BODY()

	struct FPendingEquippedWeapon
	{
		uint64 ItemId = 0;
		int32 WeaponType = Protocol::WEAPON_TYPE_NONE;
	};

public:
	UFUNCTION(BlueprintCallable, Category = "Network")
	void ConnectToGameServer(const FString& IPAddress);

	UFUNCTION(BlueprintCallable)
	void DisconnectFromGameServer();
	// 서버에서 남이 나갔다고 알려줬을 때 실행할 함수
	void HandleLeaveGame(const Protocol::S_LEAVE_GAME& pkt);

	UFUNCTION(BlueprintCallable)
	void HandleRecvPackets();

	void SendPacket(SendBufferRef SendBuffer);
	static void SendPacketStatic(SendBufferRef SendBuffer);

public:
	void HandleSpawn(const Protocol::ObjectInfo& PlayerInfo, bool IsMine);
	void HandleSpawn(const Protocol::S_ENTER_GAME& EnterGamePkt);
	void HandleSpawn(const Protocol::S_SPAWN& SpawnPkt);

	void HandleDespawn(uint64 ObjectId);
	void HandleDespawn(const Protocol::S_DESPAWN& DespawnPkt);

	void HandleMove(const Protocol::S_MOVE& MovePkt);
	void HandleEnterTruck(const Protocol::S_ENTER_TRUCK& pkt);
	void HandleExitTruck(const Protocol::S_EXIT_TRUCK& pkt);
	void HandleTruckMove(const Protocol::S_TRUCK_MOVE& pkt);
	void HandleToggleDoor(const Protocol::S_TOGGLE_DOOR& pkt);

	void HandleEquipWeapon(const Protocol::S_EQUIP_WEAPON& pkt);
	void HandleSpawnItem(const Protocol::S_SPAWN_ITEM& pkt);
	void HandleFire(const Protocol::S_FIRE& pkt);
	void ApplyEquippedWeapon(uint64 PlayerId, uint64 ItemId, int32 WeaponType);
	void RetryPendingWeapon(uint64 PlayerId);
	TSubclassOf<class AWeaponBase> ResolveWeaponClass(int32 WeaponType) const;
	class ATruck* FindTruckById(uint64 TruckId);
	class AADoor* FindDoorById(int32 DoorId);
	class AFPSBaseCharacter* ResolvePlayerById(uint64 PlayerId) const;
	void CacheTruckActors();
	void CacheDoorActors();
	bool IsConnectedToGameServer() const;
	bool ShouldUseLocalInteractionFallback() const;
	bool TryPickupWeaponLocally(class AFPSBaseCharacter* Character, class AWeaponBase* Weapon);
	bool TryEnterTruckLocally(class AFPSBaseCharacter* Character, class ATruck* Truck, Protocol::TruckSeatType SeatType);
	bool TryExitTruckLocally(class AFPSBaseCharacter* Character);
	bool IsZombieSyncAuthority() const;
	void RegisterAuthorityZombie(class ABaseZombie* Zombie);
	void ActivateAuthorityZombieSpawners();
	void RecordStage1CargoItems(const TArray<EItemType>& Items);

	UFUNCTION(BlueprintCallable, Category = "Stage1|Cargo")
	void ClearRecordedStage1CargoItems();

	UFUNCTION(BlueprintPure, Category = "Stage1|Cargo")
	int32 GetRecordedStage1CargoItemCount(EItemType ItemType) const;

public:
	virtual void Shutdown() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }

public:
	// GameServer
	class FSocket* Socket;
	//FString IpAddress = TEXT("127.0.0.1");
	int16 Port = 7777;
	TSharedPtr<class PacketSession> GameServerSession;

public:
	UPROPERTY(EditAnywhere, Category = "Network")
	TSubclassOf<class AFPSBaseCharacter> OtherPlayerClass;
	UPROPERTY(EditAnywhere, Category = "Network")
	TSubclassOf<class ABaseZombie> NetworkZombieClass;
	class AFPSBaseCharacter* MyPlayer;
	TMap<uint64, class AFPSBaseCharacter*> Players;
	TMap<uint64, class ABaseZombie*> Zombies;
	TMap<uint64, class ATruck*> Trucks;
	TMap<int32, class AADoor*> Doors;

	// [추가] 바닥에 떨어진 아이템(총기 등)들을 ID로 관리하기 위한 맵
	UPROPERTY()
	TMap<uint64, AActor*> FieldItems;

	TMap<uint64, FPendingEquippedWeapon> PendingWeaponsByPlayer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage1|Cargo")
	TMap<EItemType, int32> RecordedStage1CargoItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network|Class")
	TSubclassOf<class AWeaponBase> DefaultWeaponClass;	// 바닥에 떨어진 무기 스폰할 때 사용할 기본 무기 클래스

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network|Class")
	TSubclassOf<class AWeaponBase> DefaultEquippedWeaponClass;	// 손에 장착된 무기 스폰할 때 사용할 기본 무기 클래스

private:
	void SyncAuthorityZombiesToServer(float DeltaTime);
	uint64 AllocateZombieObjectId();
	float ZombieSyncAccumulator = 0.0f;
	uint64 NextZombieObjectId = 1000000;
};