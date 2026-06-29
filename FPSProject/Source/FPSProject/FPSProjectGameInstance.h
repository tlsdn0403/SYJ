#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Tickable.h"
#include "FPSProject.h"
#include "Enum.pb.h"
#include "Items/LootItemBase.h"
#include "FPSProjectGameInstance.generated.h"

class UUserWidget;
class ALootItemBase;
class FFPSNetworkManager;
class FFPSSpawnManager;
class FFPSStageFlowManager;
class UFPSWorldObjectManager;

/**
 *
 */

UCLASS()
class FPSPROJECT_API UFPSProjectGameInstance : public UGameInstance, public FTickableGameObject
{
	GENERATED_BODY()
	friend class FFPSSpawnManager;
	friend class FFPSStageFlowManager;

	struct FPendingEquippedWeapon
	{
		uint64 ItemId = 0;
		int32 WeaponType = Protocol::WEAPON_TYPE_NONE;
	};

public:
	UFPSProjectGameInstance();

	UFUNCTION(BlueprintCallable, Category = "Network")
	void ConnectToGameServer(const FString& IPAddress);

	UFUNCTION(BlueprintCallable)
	void DisconnectFromGameServer();

	UFUNCTION(BlueprintCallable)
	void QuitGame();

	// 서버에서 남이 나갔다고 알려줬을 때 실행할 함수
	void HandleLeaveGame(const Protocol::S_LEAVE_GAME& pkt);

	UFUNCTION(BlueprintCallable)
	void HandleRecvPackets();

	void SendPacket(SendBufferRef SendBuffer);
	static void SendPacketStatic(SendBufferRef SendBuffer);
	static bool SendZombieHitPacket(class AFPSBaseCharacter* Attacker, class ABaseZombie* Zombie, float Damage, const FVector& HitLocation, FName HitBoneName = NAME_None, const FVector& HitNormal = FVector::ZeroVector);
	void SetPlayerNickname(const FString& Nickname);
	const FString& GetPlayerNickname() const { return PlayerNickname; }
	FString GetPlayerNicknameById(uint64 PlayerId) const;
	void GetSurvivingPlayerNicknames(TArray<FString>& OutNicknames) const;

public:
	void HandleSpawn(const Protocol::ObjectInfo& PlayerInfo, bool IsMine);
	void HandleSpawn(const Protocol::S_ENTER_GAME& EnterGamePkt);
	void HandleSpawn(const Protocol::S_SPAWN& SpawnPkt);

	void HandleDespawn(uint64 ObjectId);
	void HandleDespawn(const Protocol::S_DESPAWN& DespawnPkt);

	void HandleMove(const Protocol::S_MOVE& MovePkt);
	void HandleZombieAttack(const Protocol::S_ZOMBIE_ATTACK& pkt);
	void HandleZombieHp(const Protocol::S_ZOMBIE_HP& pkt);
	void HandleZombieDie(const Protocol::S_ZOMBIE_DIE& pkt);
	void HandleZombieDismember(const Protocol::S_ZOMBIE_DISMEMBER& pkt);
	void HandleEnterTruck(const Protocol::S_ENTER_TRUCK& pkt);
	void HandleExitTruck(const Protocol::S_EXIT_TRUCK& pkt);
	void HandleTruckMove(const Protocol::S_TRUCK_MOVE& pkt);
	void HandleLoadTruckItem(const Protocol::S_LOAD_TRUCK_ITEM& pkt);
	void HandleMachineGunAmmo(const Protocol::S_MACHINE_GUN_AMMO& pkt);
	void HandleToggleDoor(const Protocol::S_TOGGLE_DOOR& pkt);
	void HandleEnterGameReadyCount(const Protocol::S_ENTER_GAME_READY_COUNT& pkt);
	void HandleStageTimer(const Protocol::S_STAGE_TIMER& pkt);
	void HandleStage1ItemSeed(const Protocol::S_STAGE1_ITEM_SEED& pkt);
	void HandleRespawnLootItem(const Protocol::S_RESPAWN_LOOT_ITEM& pkt);
	void HandleStageTransition(const Protocol::S_STAGE_TRANSITION& pkt);

	void HandleEquipWeapon(const Protocol::S_EQUIP_WEAPON& pkt);
	void HandleSpawnItem(const Protocol::S_SPAWN_ITEM& pkt);
	void HandleFire(const Protocol::S_FIRE& pkt);
	void ApplyEquippedWeapon(uint64 PlayerId, uint64 ItemId, int32 WeaponType);
	void RetryPendingWeapon(uint64 PlayerId);
	TSubclassOf<class AWeaponBase> ResolveWeaponClass(int32 WeaponType) const;
	class ATruck* FindTruckById(uint64 TruckId);
	class AADoor* FindDoorById(int32 DoorId);
	class AFPSBaseCharacter* ResolvePlayerById(uint64 PlayerId) const;
	class AFPSBaseCharacter* GetSpectateTargetBySlot(int32 SlotIndex) const;
	class ALootItemBase* FindNetworkLootItemById(uint64 LootItemId);
	void CacheTruckActors();
	void CacheDoorActors();
	bool IsConnectedToGameServer() const;
	bool ShouldUseLocalInteractionFallback() const;
	bool ShouldDelayEnterGameRequest() const;
	void RequestEnterGameWhenReady();
	void RefreshStage2StartupActorHold();
	bool TrySendEnterGamePacket();
	void SetEntryLoadingWidgetClass(TSubclassOf<UUserWidget> WidgetClass);
	void ShowEntryLoadingWidget();
	void RegisterEntryLoadingWidget(UUserWidget* Widget);
	void RemoveEntryLoadingWidget();
	bool TryPickupWeaponLocally(class AFPSBaseCharacter* Character, class AWeaponBase* Weapon);
	bool TryEnterTruckLocally(class AFPSBaseCharacter* Character, class ATruck* Truck, Protocol::TruckSeatType SeatType);
	bool TryExitTruckLocally(class AFPSBaseCharacter* Character);
	void RecordStage1CargoItems(const TArray<EItemType>& Items);
	bool ConsumeRecordedStage1CargoItem(EItemType ItemType, int32 Amount = 1);
	void RegisterNetworkLootItem(ALootItemBase* LootItem);
	void UnregisterNetworkLootItem(uint64 LootItemId);
	bool IsNetworkLootItemInactive(uint64 LootItemId) const;
	bool ShowGameOverScreen();
	void EvaluateGameOverIfAllPlayersDead();

	UFUNCTION(BlueprintCallable, Category = "Stage1|Cargo")
	void ClearRecordedStage1CargoItems();

	UFUNCTION(BlueprintPure, Category = "Stage1|Cargo")
	int32 GetRecordedStage1CargoItemCount(EItemType ItemType) const;

	UFUNCTION(BlueprintPure, Category = "Stage")
	bool IsInStage2World() const;

public:
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }

public:
	UPROPERTY(EditAnywhere, Category = "Network")
	TSubclassOf<class AFPSBaseCharacter> OtherPlayerClass;

	// Server player IDs are issued in join order: classes 1, 2 and 3 are selected in sequence.
	UPROPERTY(EditAnywhere, Category = "Network|Player")
	TArray<TSubclassOf<class AFPSBaseCharacter>> PlayerCharacterClasses;
	UPROPERTY(EditAnywhere, Category = "Network")
	TSubclassOf<class ABaseZombie> NetworkZombieClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	TArray<TSubclassOf<class ABaseZombie>> NetworkZombieClasses;
	class AFPSBaseCharacter* MyPlayer;

	TMap<uint64, FPendingEquippedWeapon> PendingWeaponsByPlayer;

	struct FPendingStage2SpawnInfo
	{
		Protocol::ObjectInfo ObjectInfo;
		bool bIsMine = false;
	};

	TArray<FPendingStage2SpawnInfo> PendingStage2SpawnInfos;
	bool bProcessingPendingStage2Spawns = false;
	bool bStage2StartupHoldApplied = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage1|Cargo")
	TMap<EItemType, int32> RecordedStage1CargoItems;

	TSet<uint64> InactiveNetworkLootItemIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network|Class")
	TSubclassOf<class AWeaponBase> DefaultWeaponClass;	// 바닥에 떨어진 무기 스폰할 때 사용할 기본 무기 클래스

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network|Class")
	TSubclassOf<class AWeaponBase> DefaultEquippedWeaponClass;	// 손에 장착된 무기 스폰할 때 사용할 기본 무기 클래스

	UPROPERTY()
	TObjectPtr<UUserWidget> EntryLoadingWidget = nullptr;

	UPROPERTY()
	TSubclassOf<UUserWidget> EntryLoadingWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|UI")
	TSubclassOf<UUserWidget> StageTransitionLoadingWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ending|UI")
	TSubclassOf<UUserWidget> GameOverWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> GameOverWidget = nullptr;

	UPROPERTY()
	FString PlayerNickname;

	TMap<uint64, FString> PlayerNicknamesById;

private:
	UFUNCTION()
	void OnGameOverExitClicked();

	UPROPERTY()
	TObjectPtr<UFPSWorldObjectManager> WorldObjects;

	int16 Port = 7777;
	TSharedPtr<FFPSNetworkManager> NetworkManager;
	TSharedPtr<FFPSSpawnManager> SpawnManager;
	TSharedPtr<FFPSStageFlowManager> StageFlowManager;

	TSubclassOf<class AFPSBaseCharacter> ResolvePlayerCharacterClass(uint64 ObjectId) const;
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void ApplyEntryLoadingReadyCount(int32 ReadyCount);
	void CachePlayerNickname(const Protocol::ObjectInfo& ObjectInfo, bool bIsMine);
	void ApplyStageTimerToLocalUI();
	void ProcessSpawnObject(const Protocol::ObjectInfo& ObjectInfo, bool IsMine);
	bool ShouldDelayStage2ActorSpawn() const;
	void QueueStage2Spawn(const Protocol::ObjectInfo& ObjectInfo, bool IsMine);
	void ProcessPendingStage2Spawns();
	void ApplyStage2StartupActorHold(bool bHold);
	void TryDistributeStage1CargoItemsToPlayers();
	bool IsRegisteredPlayer(class AFPSBaseCharacter* Player) const;
	void GetValidRegisteredPlayers(TArray<TPair<uint64, class AFPSBaseCharacter*>>& OutPlayers) const;
	void TickNetwork();
	void TickStageFlow();
	bool RemovePlayerById(uint64 PlayerId);

	bool bGameOverScreenShown = false;
};
