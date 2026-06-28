#pragma once
#include "JobQueue.h"
#include <chrono>
#include <unordered_set>
#include <vector>

class Room : public JobQueue
{
public:
	Room();
	virtual ~Room();

public:
	bool EnterRoom(ObjectRef object, bool randPos = true);
	bool LeaveRoom(ObjectRef object);

	bool HandleEnterPlayer(PlayerRef player);
	void HandleReadyPlayer(GameSessionRef session, Protocol::C_ENTER_GAME pkt);
	void HandleStageMapReady(GameSessionRef session, Protocol::C_ENTER_GAME pkt);
	void RemovePendingReadySession(GameSessionRef session);
	bool HandleLeavePlayer(PlayerRef player);
	void HandleMove(PlayerRef player, Protocol::C_MOVE pkt);
	void HandleHitZombie(PlayerRef player, Protocol::C_HIT_ZOMBIE pkt);
	void HandleEquipWeapon(PlayerRef player, Protocol::C_EQUIP_WEAPON pkt);
	void HandlePickupLootItem(PlayerRef player, Protocol::C_PICKUP_LOOT_ITEM pkt);
	void HandleFire(PlayerRef player, Protocol::C_FIRE pkt);
	void HandleEnterTruck(PlayerRef player, Protocol::C_ENTER_TRUCK pkt);
	void HandleExitTruck(PlayerRef player, Protocol::C_EXIT_TRUCK pkt);
	void HandleTruckMove(PlayerRef player, Protocol::C_TRUCK_MOVE pkt);
	void HandleLoadTruckItem(PlayerRef player, Protocol::C_LOAD_TRUCK_ITEM pkt);
	void HandleToggleDoor(PlayerRef player, Protocol::C_TOGGLE_DOOR pkt);
	void HandleStageTransitionRequest(PlayerRef player, Protocol::C_STAGE_TRANSITION_REQUEST pkt);

public:
	void UpdateTick();
	void SpawnStage2Zombies();
	void SpawnStage2Weapons();

	RoomRef GetRoomRef();

private:
	struct TruckState;
	void UpdateZombies();
	PlayerRef FindNearestPlayer(const Protocol::PosInfo& origin, float maxRange) const;
	Protocol::PosInfo GetZombieTargetPosInfo(const PlayerRef& player, const Protocol::PosInfo* zombiePos = nullptr) const;
	void BroadcastZombieMove(const MonsterRef& monster, bool force = false);
	bool ShouldBroadcastZombieMove(const MonsterRef& monster, bool force);
	void QueueStage2ZombieSpawn(
		float x,
		float y,
		float z,
		float yaw,
		Protocol::ZombieType zombieType,
		int32 tileTypeCode,
		int32 tileOccurrenceIndex);
	void ProcessPendingStage2ZombieSpawns();
	struct ZombiePathPoint
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	struct ZombiePathState
	{
		uint64 targetPlayerId = 0;
		vector<ZombiePathPoint> waypoints;
		size_t waypointIndex = 0;
		float repathRemainingSeconds = 0.0f;
		float lastTargetX = 0.0f;
		float lastTargetY = 0.0f;
		float lastTargetZ = 0.0f;
	};

	struct PendingStage2ZombieSpawn
	{
		uint64 zombieId = 0;
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float yaw = 0.0f;
		int32 encodedSpawnType = 0;
	};

	struct ZombieMoveBroadcastState
	{
		float lastX = 0.0f;
		float lastY = 0.0f;
		float lastZ = 0.0f;
		float lastYaw = 0.0f;
		float elapsedSeconds = 0.0f;
		bool hasLastMove = false;
	};

	struct ZombieAiUpdateState
	{
		float elapsedSeconds = 0.0f;
	};

	vector<ZombiePathPoint> FindZombiePath(const Protocol::PosInfo& start, const Protocol::PosInfo& goal) const;
	bool AddObject(ObjectRef object);
	bool RemoveObject(uint64 objectId);

private:
	void Broadcast(SendBufferRef sendBuffer, uint64 exceptId = 0);
	void BroadcastPendingReadyCount();
	void BroadcastStageTransitionReadyCount();
	void StartTruckLoadingPhase();
	void BroadcastStageTimer();
	void RefreshMachineGunAmmoFromCargo(TruckState& truckState);
	bool ConsumeMachineGunBullet(TruckState& truckState);
	void BroadcastMachineGunAmmo(const TruckState& truckState);
	void CaptureStage2MachineGunAmmoFromTruck(uint64 truckId);
	bool ApplyPendingStage2MachineGunAmmo(TruckState& truckState);
	void SendStageTimerToSession(const GameSessionRef& session) const;
	void SendStage1ItemSeedToSession(const GameSessionRef& session) const;
	void SendStage2WeaponsToSession(const GameSessionRef& session) const;
	int32 GetTruckLoadingPhaseRemainingSeconds() const;
	void RecordStage2TileSequence(const Protocol::C_ENTER_GAME& pkt);
	int32 GetStage2TileOccurrenceCount(int32 tileTypeCode, int32 fallbackOccurrenceCount) const;

	struct TruckState
	{
		Protocol::PosInfo posInfo;
		bool hasTransform = false;
		bool hasFuel = false;
		float fuel = -1.0f;
		std::chrono::steady_clock::time_point lastFuelItemUpdateTime{};
		bool hasHealth = false;
		float hp = 0.0f;
		float maxHp = 0.0f;
		std::chrono::steady_clock::time_point lastHealthRepairUpdateTime{};
		bool hasTurretAim = false;
		float turretYaw = 0.0f;
		float turretPitch = 0.0f;
		uint64 driverPlayerId = 0;
		//[?�우] cargo 좌석?� 1??좌석???�니???�러 명이 ?�시???????�어??set?�로 관리한??
		unordered_set<uint64> cargoPlayerIds;
		uint64 turretPlayerId = 0;
		int32 mountedAmmoCount = 0;
		int32 lastSyncedMountedAmmoCount = 0;
		int32 machineGunMaxAmmo = 100;
		int32 machineGunTotalAmmo = 0;
		int32 machineGunCurrentAmmo = 0;
	};

	struct PendingZombieDespawn
	{
		uint64 zombieId = 0;
		float remainingTime = 0.0f;
	};

	struct PendingLootItemRespawn
	{
		uint64 itemId = 0;
		float remainingTime = 0.0f;
	};

	struct Stage2WeaponState
	{
		uint64 itemId = 0;
		Protocol::PosInfo posInfo;
		Protocol::WeaponType weaponType = Protocol::WEAPON_TYPE_NONE;
		bool pickedUp = false;
	};

	TruckState* FindTruckState(uint64 truckId);
	TruckState& GetOrCreateTruckState(uint64 truckId);
	bool IsTruckSeatOccupied(const TruckState& truckState, Protocol::TruckSeatType seatType) const;
	size_t GetTruckOccupantCount(const TruckState& truckState) const;
	void SetTruckSeatOccupant(TruckState& truckState, Protocol::TruckSeatType seatType, uint64 playerId);
	void ClearTruckSeatOccupant(TruckState& truckState, Protocol::TruckSeatType seatType, uint64 playerId);
	void ClearPlayerTruckState(PlayerRef player);
	void ForceExitTruck(PlayerRef player);
	void BroadcastTruckState(const TruckState& truckState, bool isCorrection = false);
	Stage2WeaponState* FindStage2Weapon(uint64 itemId);

private:
	static constexpr size_t MAX_CARGO_OCCUPANTS = 4;
	static constexpr size_t REQUIRED_STAGE2_PLAYER_COUNT = 3;
	static constexpr int32 TRUCK_LOADING_PHASE_DURATION_SECONDS = 120;

	unordered_map<uint64, ObjectRef> _objects;
	unordered_map<uint64, TruckState> _trucks;
	unordered_map<uint64, bool> _doors;
	vector<weak_ptr<GameSession>> _pendingReadySessions;
	vector<PendingZombieDespawn> _pendingZombieDespawns;
	vector<PendingLootItemRespawn> _pendingLootItemRespawns;
	vector<PendingStage2ZombieSpawn> _pendingStage2ZombieSpawns;
	vector<Stage2WeaponState> _stage2Weapons;
	unordered_map<uint64, ZombiePathState> _zombiePaths;
	unordered_map<uint64, ZombieMoveBroadcastState> _zombieMoveBroadcastStates;
	unordered_map<uint64, ZombieAiUpdateState> _zombieAiUpdateStates;
	unordered_set<uint64> _inactiveLootItemIds;
	unordered_set<uint64> _stageTransitionReadyPlayerIds;
	vector<int32> _stage2TileTypeSequence;
	uint64 _nextStage2ZombieId = 0;
	uint64 _stageTransitionTruckId = 0;
	bool _bTruckLoadingPhaseActive = false;
	bool _bStageTransitionStarted = false;
	bool _bStage2ZombiesSpawned = false;
	bool _bHasStage2TileTypeSequence = false;
	bool _bHasPendingStage2MachineGunAmmo = false;
	int32 _pendingStage2MountedAmmoCount = 0;
	int32 _pendingStage2MachineGunMaxAmmo = 100;
	int32 _pendingStage2MachineGunTotalAmmo = 0;
	int32 _pendingStage2MachineGunCurrentAmmo = 0;
	std::chrono::steady_clock::time_point _truckLoadingPhaseEndTime;
	int32 _lastBroadcastTruckLoadingRemainingSeconds = -1;
	uint32 _stage1ItemSpawnSeed = 0;
};

extern RoomRef GRoom;