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
	void HandleReadyPlayer(GameSessionRef session);
	void HandleStageMapReady(GameSessionRef session);
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
	void UpdateZombies();
	PlayerRef FindNearestPlayer(const Protocol::PosInfo& origin, float maxRange) const;
	void BroadcastZombieMove(const MonsterRef& monster);
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

	vector<ZombiePathPoint> FindZombiePath(const Protocol::PosInfo& start, const Protocol::PosInfo& goal) const;
	bool AddObject(ObjectRef object);
	bool RemoveObject(uint64 objectId);

private:
	void Broadcast(SendBufferRef sendBuffer, uint64 exceptId = 0);
	void BroadcastPendingReadyCount();
	void BroadcastStageTransitionReadyCount();
	void StartTruckLoadingPhase();
	void BroadcastStageTimer();
	void SendStageTimerToSession(const GameSessionRef& session) const;
	void SendStage1ItemSeedToSession(const GameSessionRef& session) const;
	void SendStage2WeaponsToSession(const GameSessionRef& session) const;
	int32 GetTruckLoadingPhaseRemainingSeconds() const;

	struct TruckState
	{
		Protocol::PosInfo posInfo;
		bool hasTransform = false;
		uint64 driverPlayerId = 0;
		//[신우] cargo 좌석은 1인 좌석이 아니라 여러 명이 동시에 탈 수 있어서 set으로 관리한다.
		unordered_set<uint64> cargoPlayerIds;
		uint64 turretPlayerId = 0;
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
	//[신우] 현재 2스테이지 트럭 적재함은 최대 4명까지 타는 구조로 서버에서 제한한다.
	static constexpr size_t MAX_CARGO_OCCUPANTS = 4;
	static constexpr size_t REQUIRED_STAGE2_PLAYER_COUNT = 3;
	static constexpr int32 TRUCK_LOADING_PHASE_DURATION_SECONDS = 60;

	unordered_map<uint64, ObjectRef> _objects;
	unordered_map<uint64, TruckState> _trucks;
	unordered_map<uint64, bool> _doors;
	vector<weak_ptr<GameSession>> _pendingReadySessions;
	vector<PendingZombieDespawn> _pendingZombieDespawns;
	vector<PendingLootItemRespawn> _pendingLootItemRespawns;
	vector<Stage2WeaponState> _stage2Weapons;
	unordered_map<uint64, ZombiePathState> _zombiePaths;
	unordered_set<uint64> _inactiveLootItemIds;
	unordered_set<uint64> _stageTransitionReadyPlayerIds;
	bool _bTruckLoadingPhaseActive = false;
	bool _bStageTransitionStarted = false;
	bool _bStage2ZombiesSpawned = false;
	std::chrono::steady_clock::time_point _truckLoadingPhaseEndTime;
	int32 _lastBroadcastTruckLoadingRemainingSeconds = -1;
	uint32 _stage1ItemSpawnSeed = 0;
};

extern RoomRef GRoom;