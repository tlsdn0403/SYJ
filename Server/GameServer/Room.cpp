#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "GameSession.h"
#include "Monster.h"
#include "ObjectUtils.h"
#include <algorithm>
#include <cmath>
#include <limits>

RoomRef GRoom = make_shared<Room>();

Room::Room()
{
	_stage1ItemSpawnSeed = Utils::GetRandom<uint32>(1, (std::numeric_limits<uint32>::max)());
}

Room::~Room()
{

}

Room::TruckState* Room::FindTruckState(uint64 truckId)
{
	auto it = _trucks.find(truckId);
	if (it == _trucks.end())
		return nullptr;

	return &it->second;
}

Room::TruckState& Room::GetOrCreateTruckState(uint64 truckId)
{
	TruckState& truckState = _trucks[truckId];
	if (truckState.posInfo.object_id() == 0)
	{
		truckState.posInfo.set_object_id(truckId);
		truckState.posInfo.set_state(Protocol::MOVE_STATE_IDLE);
	}

	return truckState;
}

bool Room::IsTruckSeatOccupied(const TruckState& truckState, Protocol::TruckSeatType seatType) const
{
	switch (seatType)
	{
	case Protocol::TRUCK_SEAT_DRIVER:
		return truckState.driverPlayerId != 0;
	case Protocol::TRUCK_SEAT_CARGO:
		return truckState.cargoPlayerIds.size() >= MAX_CARGO_OCCUPANTS;
	case Protocol::TRUCK_SEAT_TURRET:
		return truckState.turretPlayerId != 0;
	default:
		return true;
	}
}

size_t Room::GetTruckOccupantCount(const TruckState& truckState) const
{
	size_t occupantCount = truckState.cargoPlayerIds.size();
	if (truckState.driverPlayerId != 0)
		++occupantCount;
	if (truckState.turretPlayerId != 0)
		++occupantCount;

	return occupantCount;
}

void Room::SetTruckSeatOccupant(TruckState& truckState, Protocol::TruckSeatType seatType, uint64 playerId)
{
	switch (seatType)
	{
	case Protocol::TRUCK_SEAT_DRIVER:
		truckState.driverPlayerId = playerId;
		break;
	case Protocol::TRUCK_SEAT_CARGO:
		truckState.cargoPlayerIds.insert(playerId);
		break;
	case Protocol::TRUCK_SEAT_TURRET:
		truckState.turretPlayerId = playerId;
		break;
	default:
		break;
	}
}

void Room::ClearTruckSeatOccupant(TruckState& truckState, Protocol::TruckSeatType seatType, uint64 playerId)
{
	switch (seatType)
	{
	case Protocol::TRUCK_SEAT_DRIVER:
		if (truckState.driverPlayerId == playerId)
			truckState.driverPlayerId = 0;
		break;
	case Protocol::TRUCK_SEAT_CARGO:
		truckState.cargoPlayerIds.erase(playerId);
		break;
	case Protocol::TRUCK_SEAT_TURRET:
		if (truckState.turretPlayerId == playerId)
			truckState.turretPlayerId = 0;
		break;
	default:
		break;
	}
}

void Room::ClearPlayerTruckState(PlayerRef player)
{
	if (player == nullptr)
		return;

	player->bIsInTruck = false;
	player->currentTruckId = 0;
	player->currentTruckSeatType = Protocol::TRUCK_SEAT_NONE;
}

void Room::ForceExitTruck(PlayerRef player)
{
	if (player == nullptr || player->bIsInTruck == false)
		return;

	const uint64 playerId = player->objectInfo->object_id();
	const uint64 truckId = player->currentTruckId;
	const Protocol::TruckSeatType seatType = player->currentTruckSeatType;

	TruckState* truckState = FindTruckState(truckId);
	if (truckState != nullptr)
		ClearTruckSeatOccupant(*truckState, seatType, playerId);

	ClearPlayerTruckState(player);

	Protocol::S_EXIT_TRUCK exitPkt;
	exitPkt.set_player_id(playerId);
	exitPkt.set_truck_id(truckId);
	exitPkt.set_seat_type(seatType);

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(exitPkt);
	Broadcast(sendBuffer);
}

void Room::BroadcastTruckState(const TruckState& truckState, bool isCorrection)
{
	if (truckState.hasTransform == false)
		return;

	Protocol::S_TRUCK_MOVE movePkt;
	movePkt.mutable_info()->CopyFrom(truckState.posInfo);
	movePkt.set_is_correction(isCorrection);
	movePkt.set_fuel(truckState.fuel);
	if (truckState.hasTurretAim)
	{
		movePkt.set_has_turret_aim(true);
		movePkt.set_turret_yaw(truckState.turretYaw);
		movePkt.set_turret_pitch(truckState.turretPitch);
	}
	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(movePkt);
	Broadcast(sendBuffer);
}

bool Room::EnterRoom(ObjectRef object, bool randPos /*= true*/)
{
	bool success = AddObject(object);

	// 플레이어 위치
	if (randPos)
	{
		object->posInfo->set_x(Utils::GetRandom(0.f, 500.0f));
		object->posInfo->set_y(Utils::GetRandom(0.f, 500.0f));
		object->posInfo->set_z(588.0f);
		object->posInfo->set_yaw(Utils::GetRandom(0.f, 100.0f));
	}

	// 입장 사실을 신입 플레이어에게 알린다
	if (auto player = dynamic_pointer_cast<Player>(object))
	{
		Protocol::S_ENTER_GAME enterGamePkt;
		enterGamePkt.set_success(success);

		Protocol::ObjectInfo* playerInfo = new Protocol::ObjectInfo();
		playerInfo->CopyFrom(*object->objectInfo);
		enterGamePkt.set_allocated_player(playerInfo);

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(enterGamePkt);
		if (auto session = player->session.lock())
			session->Send(sendBuffer);
	}

	// 입장 사실을 다른 플레이어에게 알린다
	{
		Protocol::S_SPAWN spawnPkt;

		Protocol::ObjectInfo* objectInfo = spawnPkt.add_players();
		objectInfo->CopyFrom(*object->objectInfo);

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(spawnPkt);
		Broadcast(sendBuffer, object->objectInfo->object_id());
	}

	// 기존 입장한 플레이어 목록을 신입 플레이어한테 전송해준다
	if (auto player = dynamic_pointer_cast<Player>(object))
	{
		Protocol::S_SPAWN spawnPkt;

		for (auto& item : _objects)
		{
			if (item.second->IsPlayer() == false && item.second->IsMonster() == false)
				continue;

			if (item.second == object)
				continue;

			Protocol::ObjectInfo* playerInfo = spawnPkt.add_players();
			playerInfo->CopyFrom(*item.second->objectInfo);
		}

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(spawnPkt);
		if (auto session = player->session.lock())
			session->Send(sendBuffer);
	}

	return success;
}

bool Room::LeaveRoom(ObjectRef object)
{
	if (object == nullptr)
		return false;

	const uint64 objectId = object->objectInfo->object_id();
	bool success = RemoveObject(objectId);

	// 퇴장 사실을 퇴장하는 플레이어에게 알린다
	if (auto player = dynamic_pointer_cast<Player>(object))
	{
		Protocol::S_LEAVE_GAME leaveGamePkt;

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(leaveGamePkt);
		if (auto session = player->session.lock())
			session->Send(sendBuffer);
	}

	// 퇴장 사실을 알린다
	{
		Protocol::S_DESPAWN despawnPkt;
		Protocol::DespawnInfo* despawnInfo = despawnPkt.add_despawn_infos();
		despawnInfo->set_object_id(objectId);
		despawnInfo->set_object_type(Protocol::OBJECT_TYPE_CREATURE);

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(despawnPkt);
		Broadcast(sendBuffer, objectId);

		if (auto player = dynamic_pointer_cast<Player>(object))
			if (auto session = player->session.lock())
				session->Send(sendBuffer);
	}

	return success;
}

bool Room::HandleEnterPlayer(PlayerRef player)
{
	bool success = EnterRoom(player, true);

	if (success == false)
		return false;

	if (auto session = player->session.lock())
	{
		for (const auto& doorPair : _doors)
		{
			Protocol::S_TOGGLE_DOOR doorPkt;
			doorPkt.set_door_id(doorPair.first);
			doorPkt.set_is_open(doorPair.second);

			SendBufferRef doorBuffer = ServerPacketHandler::MakeSendBuffer(doorPkt);
			session->Send(doorBuffer);
		}

		SendStageTimerToSession(session);
		SendStage1ItemSeedToSession(session);
	}

	return true;
}

bool Room::HandleLeavePlayer(PlayerRef player)
{
	if (player == nullptr) return false;

	ForceExitTruck(player);

	// 나가는 유저의 ID를 미리 기억해둠
	uint64 leaveId = player->objectInfo->object_id();

	// 기존에 만들어둔 방 퇴장 로직 실행 (서버 내부 장부에서 지우는 역할)
	bool success = LeaveRoom(player);

	// 퇴장에 실패했거나 이미 나간 유저라면 여기서 끝냄
	if (success == false) return false;

	// 방에 남아있는 다른 사람들에게 "얘 나갔다"고 소문내기!
	Protocol::S_LEAVE_GAME leavePkt;
	leavePkt.set_object_id(leaveId);

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(leavePkt);
	Broadcast(sendBuffer);

	return true;
}

void Room::HandleReadyPlayer(GameSessionRef session)
{
	if (session == nullptr || session->player.load() != nullptr)
	{
		return;
	}

	vector<weak_ptr<GameSession>> CleanedPendingSessions;
	CleanedPendingSessions.reserve(_pendingReadySessions.size() + 1);

	bool bAlreadyQueued = false;
	size_t ValidReadyCount = 0;
	for (const weak_ptr<GameSession>& PendingSessionWeak : _pendingReadySessions)
	{
		GameSessionRef PendingSession = PendingSessionWeak.lock();
		if (PendingSession == nullptr || PendingSession->player.load() != nullptr)
		{
			continue;
		}

		if (PendingSession.get() == session.get())
		{
			bAlreadyQueued = true;
		}

		CleanedPendingSessions.push_back(PendingSession);
		++ValidReadyCount;
	}

	if (!bAlreadyQueued)
	{
		CleanedPendingSessions.push_back(session);
		++ValidReadyCount;
	}

	_pendingReadySessions.swap(CleanedPendingSessions);
	BroadcastPendingReadyCount();

	if (ValidReadyCount < REQUIRED_STAGE2_PLAYER_COUNT)
	{
		return;
	}

	vector<GameSessionRef> SessionsToEnter;
	vector<weak_ptr<GameSession>> RemainingSessions;
	SessionsToEnter.reserve(REQUIRED_STAGE2_PLAYER_COUNT);

	for (const weak_ptr<GameSession>& PendingSessionWeak : _pendingReadySessions)
	{
		GameSessionRef PendingSession = PendingSessionWeak.lock();
		if (PendingSession == nullptr || PendingSession->player.load() != nullptr)
		{
			continue;
		}

		if (SessionsToEnter.size() < REQUIRED_STAGE2_PLAYER_COUNT)
		{
			SessionsToEnter.push_back(PendingSession);
		}
		else
		{
			RemainingSessions.push_back(PendingSession);
		}
	}

	_pendingReadySessions.swap(RemainingSessions);
	BroadcastPendingReadyCount();

	for (const GameSessionRef& ReadySession : SessionsToEnter)
	{
		PlayerRef player = ObjectUtils::CreatePlayer(ReadySession);
		HandleEnterPlayer(player);
	}

	if (!SessionsToEnter.empty())
	{
		StartTruckLoadingPhase();
	}
}

void Room::HandleStageMapReady(GameSessionRef session)
{
	if (session == nullptr)
		return;

	PlayerRef readyPlayer = session->player.load();
	if (readyPlayer == nullptr)
		return;

	const uint64 readyPlayerId = readyPlayer->objectInfo->object_id();
	if (_bStageTransitionStarted == false)
	{
		std::cout << "[Server][EnterGame] Duplicate C_ENTER_GAME ignored. ExistingPlayerId="
			<< readyPlayerId << std::endl;
		return;
	}

	_stageTransitionReadyPlayerIds.insert(readyPlayerId);
	BroadcastStageTransitionReadyCount();

	if (_stageTransitionReadyPlayerIds.size() < REQUIRED_STAGE2_PLAYER_COUNT)
		return;

	vector<PlayerRef> readyPlayers;
	readyPlayers.reserve(REQUIRED_STAGE2_PLAYER_COUNT);
	for (const auto& item : _objects)
	{
		PlayerRef player = dynamic_pointer_cast<Player>(item.second);
		if (player == nullptr)
			continue;

		const uint64 playerId = player->objectInfo->object_id();
		if (_stageTransitionReadyPlayerIds.find(playerId) == _stageTransitionReadyPlayerIds.end())
			continue;

		readyPlayers.push_back(player);
	}

	for (const PlayerRef& player : readyPlayers)
	{
		ClearPlayerTruckState(player);
	}
	_trucks.clear();

	for (const PlayerRef& player : readyPlayers)
	{
		if (player == nullptr)
			continue;

		GameSessionRef playerSession = player->session.lock();
		if (playerSession == nullptr)
			continue;

		Protocol::S_ENTER_GAME enterGamePkt;
		enterGamePkt.set_success(true);

		Protocol::ObjectInfo* playerInfo = new Protocol::ObjectInfo();
		playerInfo->CopyFrom(*player->objectInfo);
		enterGamePkt.set_allocated_player(playerInfo);

		playerSession->Send(ServerPacketHandler::MakeSendBuffer(enterGamePkt));

		Protocol::S_SPAWN spawnPkt;
		for (const PlayerRef& otherPlayer : readyPlayers)
		{
			if (otherPlayer == nullptr || otherPlayer == player)
				continue;

			Protocol::ObjectInfo* otherPlayerInfo = spawnPkt.add_players();
			otherPlayerInfo->CopyFrom(*otherPlayer->objectInfo);
		}

		if (spawnPkt.players_size() > 0)
			playerSession->Send(ServerPacketHandler::MakeSendBuffer(spawnPkt));
	}

	_stageTransitionReadyPlayerIds.clear();
	_bStageTransitionStarted = false;
	SpawnStage2Zombies();
	SpawnStage2Weapons();
}

void Room::RemovePendingReadySession(GameSessionRef session)
{
	if (session == nullptr)
	{
		return;
	}

	vector<weak_ptr<GameSession>> RemainingSessions;
	RemainingSessions.reserve(_pendingReadySessions.size());

	for (const weak_ptr<GameSession>& PendingSessionWeak : _pendingReadySessions)
	{
		GameSessionRef PendingSession = PendingSessionWeak.lock();
		if (PendingSession == nullptr || PendingSession->player.load() != nullptr)
		{
			continue;
		}

		if (PendingSession.get() == session.get())
		{
			continue;
		}

		RemainingSessions.push_back(PendingSession);
	}

	_pendingReadySessions.swap(RemainingSessions);
	BroadcastPendingReadyCount();
}

namespace
{
	constexpr uint64 STAGE2_WEAPON_OBJECT_ID_START = 200001;
	constexpr uint64 ZOMBIE_OBJECT_ID_START = 1000000;
	constexpr float ZOMBIE_SERVER_TICK_SECONDS = 0.1f;
	constexpr float ZOMBIE_MOVE_SPEED = 180.0f;
	constexpr float ZOMBIE_AGGRO_RANGE = 5000.0f;
	constexpr float ZOMBIE_ATTACK_RANGE = 140.0f;
	constexpr float ZOMBIE_ATTACK_COOLDOWN_SECONDS = 1.0f;
	constexpr float ZOMBIE_DESPAWN_DELAY_SECONDS = 3.0f;
	constexpr float ZOMBIE_SEPARATION_RADIUS = 180.0f;
	constexpr float ZOMBIE_SEPARATION_WEIGHT = 1.35f;
	constexpr float ZOMBIE_PATH_RECALC_SECONDS = 0.75f;
	constexpr float ZOMBIE_PATH_TARGET_REPATH_DISTANCE = 300.0f;
	constexpr float ZOMBIE_WAYPOINT_REACHED_DISTANCE = 80.0f;
	constexpr float ZOMBIE_NAV_GRID_CELL_SIZE = 300.0f;
	constexpr int32 ZOMBIE_NAV_MAX_SEARCH_NODES = 512;
	constexpr int32 STAGE2_ZOMBIE_TILE_WORLD = 0;
	constexpr int32 STAGE2_ZOMBIE_TILE_STRAIGHT = 1;
	constexpr int32 STAGE2_ZOMBIE_TILE_RIGHT = 3;
	constexpr int32 STAGE2_STRAIGHT_TILE_OCCURRENCE_COUNT = 3;
	constexpr int32 STAGE2_RIGHT_TILE_OCCURRENCE_COUNT = 3;

	struct ZombieNavCell
	{
		int32 x = 0;
		int32 y = 0;
	};

	struct ZombieAStarNode
	{
		ZombieNavCell cell;
		float gCost = 0.0f;
		float fCost = 0.0f;
		int64 parentKey = 0;
		bool hasParent = false;
		bool closed = false;
	};

	struct ZombieAStarOpenNode
	{
		int64 key = 0;
		float fCost = 0.0f;

		bool operator<(const ZombieAStarOpenNode& other) const
		{
			return fCost > other.fCost;
		}
	};

	int64 MakeZombieNavCellKey(int32 x, int32 y)
	{
		return (static_cast<int64>(x) << 32) ^ static_cast<uint32>(y);
	}

	ZombieNavCell WorldToZombieNavCell(float x, float y)
	{
		return
		{
			static_cast<int32>(floorf(x / ZOMBIE_NAV_GRID_CELL_SIZE)),
			static_cast<int32>(floorf(y / ZOMBIE_NAV_GRID_CELL_SIZE))
		};
	}

	float ZombieNavCellCenterX(int32 cellX)
	{
		return (static_cast<float>(cellX) + 0.5f) * ZOMBIE_NAV_GRID_CELL_SIZE;
	}

	float ZombieNavCellCenterY(int32 cellY)
	{
		return (static_cast<float>(cellY) + 0.5f) * ZOMBIE_NAV_GRID_CELL_SIZE;
	}

	float GetZombieNavHeuristic(const ZombieNavCell& from, const ZombieNavCell& to)
	{
		const float dx = static_cast<float>(from.x - to.x);
		const float dy = static_cast<float>(from.y - to.y);
		return sqrtf(dx * dx + dy * dy);
	}

	bool IsStage2ZombieNavCellBlocked(const ZombieNavCell& cell)
	{
		// Hook exported map collision or hand-authored blocked cells here.
		(void)cell;
		return false;
	}

	constexpr int32 EncodeStage2ZombieSpawnType(Protocol::ZombieType zombieType, int32 tileTypeCode, int32 tileOccurrenceIndex)
	{
		if (tileTypeCode == STAGE2_ZOMBIE_TILE_WORLD)
		{
			return static_cast<int32>(zombieType);
		}

		return tileTypeCode * 100 + tileOccurrenceIndex * 10 + static_cast<int32>(zombieType);
	}

	constexpr Protocol::ZombieType DecodeStage2ZombieType(int32 encodedSpawnType)
	{
		const int32 zombieTypeValue = encodedSpawnType >= 10 ? encodedSpawnType % 10 : encodedSpawnType;
		switch (zombieTypeValue)
		{
		case Protocol::ZOMBIE_TYPE_MELEE:
			return Protocol::ZOMBIE_TYPE_MELEE;
		case Protocol::ZOMBIE_TYPE_RANGED:
			return Protocol::ZOMBIE_TYPE_RANGED;
		case Protocol::ZOMBIE_TYPE_TANKER:
			return Protocol::ZOMBIE_TYPE_TANKER;
		default:
			return Protocol::ZOMBIE_TYPE_MELEE;
		}
	}

	constexpr uint32 MixStage2ZombieSeed(uint32 seed, int32 index)
	{
		uint32 value = seed ^ (static_cast<uint32>(index) * 0x9E3779B9u);
		value ^= value >> 16;
		value *= 0x7FEB352Du;
		value ^= value >> 15;
		value *= 0x846CA68Bu;
		value ^= value >> 16;
		return value;
	}

	constexpr Protocol::ZombieType PickStage2ZombieType(uint32 seed, int32 index)
	{
		switch (MixStage2ZombieSeed(seed, index) % 3)
		{
		case 0:
			return Protocol::ZOMBIE_TYPE_MELEE;
		case 1:
			return Protocol::ZOMBIE_TYPE_RANGED;
		default:
			return Protocol::ZOMBIE_TYPE_TANKER;
		}
	}

	constexpr float PickStage2ZombieYaw(uint32 seed, int32 index)
	{
		return static_cast<float>(MixStage2ZombieSeed(seed, index) % 360);
	}

	struct ZombieSpawnGroupInfo
	{
		float centerX;
		float centerY;
		float centerZ;
		int32 columns;
		int32 rows;
		float spacingX;
		float spacingY;
		int32 tileTypeCode;
		int32 tileOccurrenceCount;
		uint32 typeSeed;
		uint32 yawSeed;
	};

	constexpr ZombieSpawnGroupInfo STAGE2_ZOMBIE_GROUPS[] =
	{
		{ -1810.0f, 16720.0f, 240.0f, 8, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_STRAIGHT, STAGE2_STRAIGHT_TILE_OCCURRENCE_COUNT, 0xA24BAED5u, 0x9FB21C63u },
		{ -2010.0f, 9730.0f, 300.0f, 8, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_STRAIGHT, STAGE2_STRAIGHT_TILE_OCCURRENCE_COUNT, 0xC13FA9A9u, 0x5D588B65u },
		{ -4840.0f, 10210.0f, 310.0f, 8, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_STRAIGHT, STAGE2_STRAIGHT_TILE_OCCURRENCE_COUNT, 0x91E10DA5u, 0xB7E15162u },
		{ -1770.0f, 1060.0f, 170.0f, 8, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_STRAIGHT, STAGE2_STRAIGHT_TILE_OCCURRENCE_COUNT, 0x7F4A7C15u, 0xD1B54A32u },
		{ -4570.0f, -19510.0f, 240.0f, 8, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_STRAIGHT, STAGE2_STRAIGHT_TILE_OCCURRENCE_COUNT, 0x3C6EF372u, 0xBB67AE85u },
		{ 4770.0f, 5360.0f, 50.0f, 8, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_RIGHT, STAGE2_RIGHT_TILE_OCCURRENCE_COUNT, 0x41C64E6Du, 0xA341316Cu },
		{ 15280.0f, 2310.0f, 230.0, 8, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_RIGHT, STAGE2_RIGHT_TILE_OCCURRENCE_COUNT, 0x6D2B79F5u, 0x13A5C89Bu },
		{ 21570.8f, 5466.7f, -367.0f, 5, 4, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_RIGHT, STAGE2_RIGHT_TILE_OCCURRENCE_COUNT, 0x9E3779B9u, 0xC2B2AE35u },
		{ 25180.0f, 10670.0f, 380.0, 5, 4, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_RIGHT, STAGE2_RIGHT_TILE_OCCURRENCE_COUNT, 0x85EBCA6Bu, 0x27D4EB2Fu },
		{ 19390.0f, 12080.0f, 310.0f, 8, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_RIGHT, STAGE2_RIGHT_TILE_OCCURRENCE_COUNT, 0x165667B1u, 0xD3A2646Cu },
		{ 16390.0f, 12830.0f, 260.0f, 5, 4, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_RIGHT, STAGE2_RIGHT_TILE_OCCURRENCE_COUNT, 0x27D4EB2Du, 0x94D049BBu },
	};

	struct Stage2WeaponSpawnInfo
	{
		float x;
		float y;
		float z;
		float yaw;
	};

	constexpr Stage2WeaponSpawnInfo STAGE2_WEAPON_SPAWNS[] =
	{
		{ 120.0f, 0.0f, 0.0f, 0.0f },
		{ 120.0f, 0.0f, 0.0f, 0.0f },
		{ 120.0f, 0.0f, 0.0f, 0.0f },
	};
}

void Room::SendStage2WeaponsToSession(const GameSessionRef& session) const
{
	if (session == nullptr || _stage2Weapons.empty())
		return;

	Protocol::S_SPAWN_ITEM spawnItemPkt;
	for (const Stage2WeaponState& weaponState : _stage2Weapons)
	{
		if (weaponState.pickedUp)
			continue;

		Protocol::ObjectInfo* weaponInfo = spawnItemPkt.add_items();
		weaponInfo->set_object_id(weaponState.itemId);
		weaponInfo->set_object_type(Protocol::OBJECT_TYPE_ITEM);
		weaponInfo->mutable_pos_info()->CopyFrom(weaponState.posInfo);
		weaponInfo->set_weapon_type(weaponState.weaponType);
	}

	if (spawnItemPkt.items_size() > 0)
		session->Send(ServerPacketHandler::MakeSendBuffer(spawnItemPkt));
}

void Room::SpawnStage2Weapons()
{
	if (!_stage2Weapons.empty())
		return;

	_stage2Weapons.reserve(sizeof(STAGE2_WEAPON_SPAWNS) / sizeof(STAGE2_WEAPON_SPAWNS[0]));
	for (int32 i = 0; i < static_cast<int32>(sizeof(STAGE2_WEAPON_SPAWNS) / sizeof(STAGE2_WEAPON_SPAWNS[0])); ++i)
	{
		const Stage2WeaponSpawnInfo& spawnInfo = STAGE2_WEAPON_SPAWNS[i];
		Stage2WeaponState& weaponState = _stage2Weapons.emplace_back();
		weaponState.itemId = STAGE2_WEAPON_OBJECT_ID_START + i;
		weaponState.weaponType = Protocol::WEAPON_TYPE_RIFLE;
		weaponState.pickedUp = false;
		weaponState.posInfo.set_object_id(weaponState.itemId);
		weaponState.posInfo.set_x(spawnInfo.x);
		weaponState.posInfo.set_y(spawnInfo.y);
		weaponState.posInfo.set_z(spawnInfo.z);
		weaponState.posInfo.set_yaw(spawnInfo.yaw);
		weaponState.posInfo.set_state(Protocol::MOVE_STATE_IDLE);
	}

	Protocol::S_SPAWN_ITEM spawnItemPkt;
	for (const Stage2WeaponState& weaponState : _stage2Weapons)
	{
		Protocol::ObjectInfo* weaponInfo = spawnItemPkt.add_items();
		weaponInfo->set_object_id(weaponState.itemId);
		weaponInfo->set_object_type(Protocol::OBJECT_TYPE_ITEM);
		weaponInfo->mutable_pos_info()->CopyFrom(weaponState.posInfo);
		weaponInfo->set_weapon_type(weaponState.weaponType);
	}

	Broadcast(ServerPacketHandler::MakeSendBuffer(spawnItemPkt));
	cout << "[Stage2Weapon] SpawnStage2Weapons count=" << _stage2Weapons.size() << endl;
}

void Room::SpawnStage2Zombies()
{
	if (_bStage2ZombiesSpawned)
		return;

	_bStage2ZombiesSpawned = true;

	uint64 zombieId = ZOMBIE_OBJECT_ID_START;
	auto spawnZombie = [this, &zombieId](
		float x,
		float y,
		float z,
		float yaw,
		Protocol::ZombieType zombieType,
		int32 tileTypeCode,
		int32 tileOccurrenceIndex)
	{
		MonsterRef zombie = ObjectUtils::CreateMonster(zombieId++);
		zombie->posInfo->set_x(x);
		zombie->posInfo->set_y(y);
		zombie->posInfo->set_z(z);
		zombie->posInfo->set_yaw(yaw);
		zombie->posInfo->set_state(Protocol::MOVE_STATE_IDLE);
		zombie->objectInfo->mutable_pos_info()->CopyFrom(*zombie->posInfo);
		zombie->objectInfo->set_weapon_type(EncodeStage2ZombieSpawnType(zombieType, tileTypeCode, tileOccurrenceIndex));

		EnterRoom(zombie, false);
	};

	for (const ZombieSpawnGroupInfo& groupInfo : STAGE2_ZOMBIE_GROUPS)
	{
		const float startX = groupInfo.centerX - (static_cast<float>(groupInfo.columns - 1) * groupInfo.spacingX * 0.5f);
		const float startY = groupInfo.centerY - (static_cast<float>(groupInfo.rows - 1) * groupInfo.spacingY * 0.5f);

		for (int32 occurrenceIndex = 0; occurrenceIndex < groupInfo.tileOccurrenceCount; ++occurrenceIndex)
		{
			for (int32 row = 0; row < groupInfo.rows; ++row)
			{
				for (int32 column = 0; column < groupInfo.columns; ++column)
				{
					const int32 spawnIndex = occurrenceIndex * groupInfo.rows * groupInfo.columns + row * groupInfo.columns + column;
					spawnZombie(
						startX + static_cast<float>(column) * groupInfo.spacingX,
						startY + static_cast<float>(row) * groupInfo.spacingY,
						groupInfo.centerZ,
						PickStage2ZombieYaw(groupInfo.yawSeed, spawnIndex),
						PickStage2ZombieType(groupInfo.typeSeed, spawnIndex),
						groupInfo.tileTypeCode,
						occurrenceIndex);
				}
			}
		}
	}
}

Room::Stage2WeaponState* Room::FindStage2Weapon(uint64 itemId)
{
	for (Stage2WeaponState& weaponState : _stage2Weapons)
	{
		if (weaponState.itemId == itemId)
			return &weaponState;
	}

	return nullptr;
}

PlayerRef Room::FindNearestPlayer(const Protocol::PosInfo& origin, float maxRange) const
{
	PlayerRef nearestPlayer = nullptr;
	float nearestDistSq = maxRange * maxRange;

	for (const auto& item : _objects)
	{
		PlayerRef player = dynamic_pointer_cast<Player>(item.second);
		if (player == nullptr)
			continue;

		if (player->posInfo->state() == Protocol::MOVE_STATE_DEAD)
			continue;

		const float dx = player->posInfo->x() - origin.x();
		const float dy = player->posInfo->y() - origin.y();
		const float dz = player->posInfo->z() - origin.z();
		const float distSq = dx * dx + dy * dy + dz * dz;
		if (distSq > nearestDistSq)
			continue;

		nearestDistSq = distSq;
		nearestPlayer = player;
	}

	return nearestPlayer;
}

void Room::BroadcastZombieMove(const MonsterRef& monster)
{
	if (monster == nullptr)
		return;

	Protocol::S_MOVE movePkt;
	movePkt.mutable_info()->CopyFrom(*monster->posInfo);

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(movePkt);
	Broadcast(sendBuffer);
}

vector<Room::ZombiePathPoint> Room::FindZombiePath(const Protocol::PosInfo& start, const Protocol::PosInfo& goal) const
{
	vector<ZombiePathPoint> fallbackPath;
	fallbackPath.push_back({ goal.x(), goal.y(), goal.z() });

	const ZombieNavCell startCell = WorldToZombieNavCell(start.x(), start.y());
	const ZombieNavCell goalCell = WorldToZombieNavCell(goal.x(), goal.y());
	if (startCell.x == goalCell.x && startCell.y == goalCell.y)
		return fallbackPath;

	priority_queue<ZombieAStarOpenNode> openList;
	unordered_map<int64, ZombieAStarNode> nodes;

	const int64 startKey = MakeZombieNavCellKey(startCell.x, startCell.y);
	ZombieAStarNode& startNode = nodes[startKey];
	startNode.cell = startCell;
	startNode.gCost = 0.0f;
	startNode.fCost = GetZombieNavHeuristic(startCell, goalCell);
	openList.push({ startKey, startNode.fCost });

	constexpr int32 neighborCount = 8;
	const int32 neighborX[neighborCount] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	const int32 neighborY[neighborCount] = { 0, 0, 1, -1, 1, -1, 1, -1 };

	int32 searchedNodeCount = 0;
	int64 goalKey = 0;
	bool foundGoal = false;

	while (openList.empty() == false && searchedNodeCount < ZOMBIE_NAV_MAX_SEARCH_NODES)
	{
		const ZombieAStarOpenNode openNode = openList.top();
		openList.pop();

		auto nodeIt = nodes.find(openNode.key);
		if (nodeIt == nodes.end())
			continue;

		ZombieAStarNode& currentNode = nodeIt->second;
		if (currentNode.closed)
			continue;

		currentNode.closed = true;
		++searchedNodeCount;

		if (currentNode.cell.x == goalCell.x && currentNode.cell.y == goalCell.y)
		{
			goalKey = openNode.key;
			foundGoal = true;
			break;
		}

		for (int32 index = 0; index < neighborCount; ++index)
		{
			const ZombieNavCell neighborCell =
			{
				currentNode.cell.x + neighborX[index],
				currentNode.cell.y + neighborY[index]
			};

			if (IsStage2ZombieNavCellBlocked(neighborCell))
				continue;

			const bool diagonal = neighborX[index] != 0 && neighborY[index] != 0;
			const float moveCost = diagonal ? 1.41421356f : 1.0f;
			const float nextGCost = currentNode.gCost + moveCost;
			const int64 neighborKey = MakeZombieNavCellKey(neighborCell.x, neighborCell.y);

			ZombieAStarNode& neighborNode = nodes[neighborKey];
			if (neighborNode.closed)
				continue;

			if (neighborNode.hasParent == false && neighborKey != startKey)
			{
				neighborNode.cell = neighborCell;
				neighborNode.gCost = nextGCost;
				neighborNode.fCost = nextGCost + GetZombieNavHeuristic(neighborCell, goalCell);
				neighborNode.parentKey = openNode.key;
				neighborNode.hasParent = true;
				openList.push({ neighborKey, neighborNode.fCost });
				continue;
			}

			if (nextGCost >= neighborNode.gCost)
				continue;

			neighborNode.cell = neighborCell;
			neighborNode.gCost = nextGCost;
			neighborNode.fCost = nextGCost + GetZombieNavHeuristic(neighborCell, goalCell);
			neighborNode.parentKey = openNode.key;
			neighborNode.hasParent = true;
			openList.push({ neighborKey, neighborNode.fCost });
		}
	}

	if (foundGoal == false)
		return fallbackPath;

	vector<ZombieNavCell> cells;
	int64 currentKey = goalKey;
	while (true)
	{
		auto nodeIt = nodes.find(currentKey);
		if (nodeIt == nodes.end())
			return fallbackPath;

		cells.push_back(nodeIt->second.cell);
		if (currentKey == startKey)
			break;

		if (nodeIt->second.hasParent == false)
			return fallbackPath;

		currentKey = nodeIt->second.parentKey;
	}

	reverse(cells.begin(), cells.end());
	if (cells.size() <= 1)
		return fallbackPath;

	vector<ZombiePathPoint> path;
	path.reserve(cells.size());
	for (size_t index = 1; index < cells.size(); ++index)
	{
		const float alpha = static_cast<float>(index) / static_cast<float>(cells.size() - 1);
		path.push_back(
		{
			ZombieNavCellCenterX(cells[index].x),
			ZombieNavCellCenterY(cells[index].y),
			start.z() + (goal.z() - start.z()) * alpha
		});
	}

	path.back() = { goal.x(), goal.y(), goal.z() };
	return path;
}

void Room::UpdateZombies()
{
	for (auto& item : _objects)
	{
		MonsterRef monster = dynamic_pointer_cast<Monster>(item.second);
		if (monster == nullptr)
			continue;

		if (monster->IsDead())
		{
			_zombiePaths.erase(item.first);
			continue;
		}

		if (monster->objectInfo->weapon_type() >= 10)
		{
			// Tile-local zombies become real world zombies after a client resolves
			// their tile transform and sends a placement correction.
			continue;
		}

		monster->TickCooldown(ZOMBIE_SERVER_TICK_SECONDS);

		PlayerRef targetPlayer = FindNearestPlayer(*monster->posInfo, ZOMBIE_AGGRO_RANGE);
		if (targetPlayer == nullptr)
		{
			_zombiePaths.erase(item.first);
			if (monster->posInfo->state() != Protocol::MOVE_STATE_IDLE)
			{
				monster->posInfo->set_state(Protocol::MOVE_STATE_IDLE);
				monster->objectInfo->mutable_pos_info()->CopyFrom(*monster->posInfo);
				BroadcastZombieMove(monster);
			}
			continue;
		}

		const float dx = targetPlayer->posInfo->x() - monster->posInfo->x();
		const float dy = targetPlayer->posInfo->y() - monster->posInfo->y();
		const float dz = targetPlayer->posInfo->z() - monster->posInfo->z();
		const float distSq = dx * dx + dy * dy + dz * dz;
		const float attackRangeSq = ZOMBIE_ATTACK_RANGE * ZOMBIE_ATTACK_RANGE;
		float separationX = 0.0f;
		float separationY = 0.0f;
		const float separationRadiusSq = ZOMBIE_SEPARATION_RADIUS * ZOMBIE_SEPARATION_RADIUS;

		for (const auto& otherItem : _objects)
		{
			if (otherItem.first == item.first)
				continue;

			MonsterRef otherMonster = dynamic_pointer_cast<Monster>(otherItem.second);
			if (otherMonster == nullptr || otherMonster->IsDead())
				continue;

			const float awayX = monster->posInfo->x() - otherMonster->posInfo->x();
			const float awayY = monster->posInfo->y() - otherMonster->posInfo->y();
			const float otherDistSq = awayX * awayX + awayY * awayY;
			if (otherDistSq >= separationRadiusSq)
				continue;

			if (otherDistSq <= 0.001f)
			{
				const float fallbackAngle = static_cast<float>((item.first * 37 + otherItem.first * 17) % 360) * (3.1415926535f / 180.0f);
				separationX += cosf(fallbackAngle);
				separationY += sinf(fallbackAngle);
				continue;
			}

			const float otherDist = sqrtf(otherDistSq);
			const float strength = (ZOMBIE_SEPARATION_RADIUS - otherDist) / ZOMBIE_SEPARATION_RADIUS;
			separationX += (awayX / otherDist) * strength;
			separationY += (awayY / otherDist) * strength;
		}

		if (distSq <= attackRangeSq)
		{
			monster->posInfo->set_state(Protocol::MOVE_STATE_IDLE);
			const float separationSq = separationX * separationX + separationY * separationY;
			if (separationSq > 0.001f)
			{
				const float separationLen = sqrtf(separationSq);
				const float moveStep = ZOMBIE_MOVE_SPEED * monster->GetMoveSpeedScale() * ZOMBIE_SERVER_TICK_SECONDS;
				monster->posInfo->set_x(monster->posInfo->x() + (separationX / separationLen) * moveStep);
				monster->posInfo->set_y(monster->posInfo->y() + (separationY / separationLen) * moveStep);
			}

			if (monster->CanAttack())
			{
				Protocol::S_ZOMBIE_ATTACK attackPkt;
				attackPkt.set_zombie_id(monster->objectInfo->object_id());
				attackPkt.set_target_player_id(targetPlayer->objectInfo->object_id());

				SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(attackPkt);
				Broadcast(sendBuffer);
				monster->StartAttackCooldown(ZOMBIE_ATTACK_COOLDOWN_SECONDS);
			}
		}
		else
		{
			ZombiePathState& pathState = _zombiePaths[item.first];
			pathState.repathRemainingSeconds -= ZOMBIE_SERVER_TICK_SECONDS;

			const float targetMoveX = targetPlayer->posInfo->x() - pathState.lastTargetX;
			const float targetMoveY = targetPlayer->posInfo->y() - pathState.lastTargetY;
			const float targetMoveZ = targetPlayer->posInfo->z() - pathState.lastTargetZ;
			const float targetMoveDistSq = targetMoveX * targetMoveX + targetMoveY * targetMoveY + targetMoveZ * targetMoveZ;
			const bool shouldRepath =
				pathState.targetPlayerId != targetPlayer->objectInfo->object_id() ||
				pathState.waypoints.empty() ||
				pathState.waypointIndex >= pathState.waypoints.size() ||
				pathState.repathRemainingSeconds <= 0.0f ||
				targetMoveDistSq >= ZOMBIE_PATH_TARGET_REPATH_DISTANCE * ZOMBIE_PATH_TARGET_REPATH_DISTANCE;

			if (shouldRepath)
			{
				pathState.targetPlayerId = targetPlayer->objectInfo->object_id();
				pathState.waypoints = FindZombiePath(*monster->posInfo, *targetPlayer->posInfo);
				pathState.waypointIndex = 0;
				pathState.repathRemainingSeconds = ZOMBIE_PATH_RECALC_SECONDS;
				pathState.lastTargetX = targetPlayer->posInfo->x();
				pathState.lastTargetY = targetPlayer->posInfo->y();
				pathState.lastTargetZ = targetPlayer->posInfo->z();
			}

			while (pathState.waypointIndex < pathState.waypoints.size())
			{
				const ZombiePathPoint& waypoint = pathState.waypoints[pathState.waypointIndex];
				const float waypointDx = waypoint.x - monster->posInfo->x();
				const float waypointDy = waypoint.y - monster->posInfo->y();
				const float waypointDz = waypoint.z - monster->posInfo->z();
				const float waypointDistSq = waypointDx * waypointDx + waypointDy * waypointDy + waypointDz * waypointDz;
				if (waypointDistSq > ZOMBIE_WAYPOINT_REACHED_DISTANCE * ZOMBIE_WAYPOINT_REACHED_DISTANCE)
					break;

				++pathState.waypointIndex;
			}

			ZombiePathPoint moveTarget = { targetPlayer->posInfo->x(), targetPlayer->posInfo->y(), targetPlayer->posInfo->z() };
			if (pathState.waypointIndex < pathState.waypoints.size())
				moveTarget = pathState.waypoints[pathState.waypointIndex];

			const float pathDx = moveTarget.x - monster->posInfo->x();
			const float pathDy = moveTarget.y - monster->posInfo->y();
			const float pathDz = moveTarget.z - monster->posInfo->z();
			const float pathDistSq = pathDx * pathDx + pathDy * pathDy + pathDz * pathDz;
			const float distance = sqrtf(pathDistSq);
			if (distance > 0.001f)
			{
				const float moveStep = ZOMBIE_MOVE_SPEED * monster->GetMoveSpeedScale() * ZOMBIE_SERVER_TICK_SECONDS;
				float moveX = pathDx / distance;
				float moveY = pathDy / distance;
				const float separationSq = separationX * separationX + separationY * separationY;
				if (separationSq > 0.001f)
				{
					const float separationLen = sqrtf(separationSq);
					moveX += (separationX / separationLen) * ZOMBIE_SEPARATION_WEIGHT;
					moveY += (separationY / separationLen) * ZOMBIE_SEPARATION_WEIGHT;
				}

				const float moveLenSq = moveX * moveX + moveY * moveY;
				if (moveLenSq > 0.001f)
				{
					const float moveLen = sqrtf(moveLenSq);
					moveX /= moveLen;
					moveY /= moveLen;
				}

				const float moveAlpha = (moveStep < distance) ? moveStep : distance;

				monster->posInfo->set_x(monster->posInfo->x() + moveX * moveAlpha);
				monster->posInfo->set_y(monster->posInfo->y() + moveY * moveAlpha);
				monster->posInfo->set_z(monster->posInfo->z() + pathDz * ((moveAlpha < distance) ? (moveAlpha / distance) : 1.0f));
				monster->posInfo->set_yaw(atan2f(moveY, moveX) * (180.0f / 3.1415926535f));
			}

			monster->posInfo->set_state(Protocol::MOVE_STATE_RUN);
		}

		monster->objectInfo->mutable_pos_info()->CopyFrom(*monster->posInfo);
		BroadcastZombieMove(monster);
	}
}

void Room::HandleMove(PlayerRef player, Protocol::C_MOVE pkt)
{
	const uint64 objectId = pkt.info().object_id();
	if (player == nullptr || player->objectInfo == nullptr)
	{
		return;
	}

	if (objectId >= ZOMBIE_OBJECT_ID_START)
	{
		auto findIt = _objects.find(objectId);
		if (findIt == _objects.end())
			return;

		MonsterRef monster = dynamic_pointer_cast<Monster>(findIt->second);
		if (monster == nullptr || monster->IsDead())
			return;

		const int32 encodedSpawnType = monster->objectInfo->weapon_type();
		if (encodedSpawnType < 10)
			return;

		if (!std::isfinite(pkt.info().x()) ||
			!std::isfinite(pkt.info().y()) ||
			!std::isfinite(pkt.info().z()) ||
			!std::isfinite(pkt.info().yaw()))
		{
			return;
		}

		monster->posInfo->CopyFrom(pkt.info());
		monster->posInfo->set_state(Protocol::MOVE_STATE_IDLE);
		monster->objectInfo->mutable_pos_info()->CopyFrom(*monster->posInfo);
		monster->objectInfo->set_weapon_type(static_cast<int32>(DecodeStage2ZombieType(encodedSpawnType)));
		BroadcastZombieMove(monster);
		return;
	}

	if (objectId != player->objectInfo->object_id())
	{
		return;
	}

	if (_objects.find(objectId) == _objects.end())
		return;

	// 적용
	player = dynamic_pointer_cast<Player>(_objects[objectId]);
	if (player == nullptr)
		return;

	if (player->posInfo->state() == Protocol::MOVE_STATE_DEAD &&
		pkt.info().state() != Protocol::MOVE_STATE_DEAD)
	{
		return;
	}

	if (pkt.info().state() == Protocol::MOVE_STATE_DEAD)
	{
		ForceExitTruck(player);
		player->posInfo->CopyFrom(pkt.info());
		player->objectInfo->mutable_pos_info()->CopyFrom(*player->posInfo);

		Protocol::S_MOVE movePkt;
		movePkt.mutable_info()->CopyFrom(pkt.info());

		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(movePkt);
		Broadcast(sendBuffer);
		return;
	}

	if (player->bIsInTruck &&
		player->currentTruckSeatType == Protocol::TRUCK_SEAT_TURRET)
	{
		// Mounted-gun users still send normal character movement from their pawn.
		// Only packets carrying the turret marker are allowed through this branch.
		constexpr float TURRET_AIM_PACKET_MARKER = 1.0f;
		if (std::abs(pkt.info().roll() - TURRET_AIM_PACKET_MARKER) > 0.001f ||
			!std::isfinite(pkt.info().yaw()) ||
			!std::isfinite(pkt.info().pitch()))
		{
			return;
		}

		Protocol::S_MOVE aimPkt;
		aimPkt.mutable_info()->CopyFrom(pkt.info());
		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(aimPkt);
		Broadcast(sendBuffer);
		return;
	}

	const bool bShouldIgnoreMoveWhileInTruck =
		player->bIsInTruck &&
		player->currentTruckSeatType != Protocol::TRUCK_SEAT_CARGO;

	//[신우] 운전석/기관총 좌석은 서버가 좌석 기준 위치를 따로 관리하므로 일반 이동 패킷을 무시한다.
	//[신우] cargo만 예외로 두는 이유는 적재함 위를 플레이어가 직접 걸어다닐 수 있기 때문이다.
	if (bShouldIgnoreMoveWhileInTruck)
		return;

	player->posInfo->CopyFrom(pkt.info());

	// 이동 사실을 알린다 (본인 포함? 빼고?)
	{
		Protocol::S_MOVE movePkt;
		{
			Protocol::PosInfo* info = movePkt.mutable_info();
			info->CopyFrom(pkt.info());
		}
		SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(movePkt);
		Broadcast(sendBuffer);
	}
}

void Room::HandleHitZombie(PlayerRef player, Protocol::C_HIT_ZOMBIE pkt)
{
	if (player == nullptr)
		return;

	const uint64 zombieId = pkt.zombie_id();
	if (zombieId < ZOMBIE_OBJECT_ID_START)
		return;

	auto findIt = _objects.find(zombieId);
	if (findIt == _objects.end())
		return;

	MonsterRef monster = dynamic_pointer_cast<Monster>(findIt->second);
	if (monster == nullptr || monster->IsDead())
		return;

	const float damage = (pkt.damage() > 0.0f) ? pkt.damage() : 0.0f;
	if (damage <= 0.0f)
		return;

	monster->ApplyDamage(damage);
	std::string brokenBoneName;
	if (monster->ApplyBoneDamage(pkt.hit_bone_name(), damage, brokenBoneName))
	{
		Protocol::S_ZOMBIE_DISMEMBER dismemberPkt;
		dismemberPkt.set_zombie_id(zombieId);
		dismemberPkt.set_bone_name(brokenBoneName);
		dismemberPkt.set_hit_x(pkt.hit_x());
		dismemberPkt.set_hit_y(pkt.hit_y());
		dismemberPkt.set_hit_z(pkt.hit_z());
		dismemberPkt.set_impulse_x(-pkt.hit_normal_x() * 300.0f);
		dismemberPkt.set_impulse_y(-pkt.hit_normal_y() * 300.0f);
		dismemberPkt.set_impulse_z(-pkt.hit_normal_z() * 300.0f);

		SendBufferRef dismemberBuffer = ServerPacketHandler::MakeSendBuffer(dismemberPkt);
		Broadcast(dismemberBuffer);

		if (brokenBoneName == "head" || brokenBoneName == "spine_01")
		{
			monster->ApplyDamage(monster->GetMaxHp());
		}
	}

	Protocol::S_ZOMBIE_HP hpPkt;
	hpPkt.set_zombie_id(zombieId);
	hpPkt.set_hp(monster->GetHp());
	hpPkt.set_max_hp(monster->GetMaxHp());

	SendBufferRef hpBuffer = ServerPacketHandler::MakeSendBuffer(hpPkt);
	Broadcast(hpBuffer);

	if (monster->IsDead() == false)
		return;

	_zombiePaths.erase(zombieId);
	monster->posInfo->set_state(Protocol::MOVE_STATE_DEAD);
	monster->objectInfo->mutable_pos_info()->CopyFrom(*monster->posInfo);

	Protocol::S_ZOMBIE_DIE diePkt;
	diePkt.set_zombie_id(zombieId);
	diePkt.set_killer_id(player->objectInfo->object_id());

	SendBufferRef dieBuffer = ServerPacketHandler::MakeSendBuffer(diePkt);
	Broadcast(dieBuffer);
	_pendingZombieDespawns.push_back({ zombieId, ZOMBIE_DESPAWN_DELAY_SECONDS });
}

void Room::HandleEquipWeapon(PlayerRef player, Protocol::C_EQUIP_WEAPON pkt)
{
	if (player == nullptr)
		return;

	const uint64 itemObjectId = pkt.itemobjectid();
	Stage2WeaponState* weaponState = FindStage2Weapon(itemObjectId);
	if (weaponState == nullptr)
	{
		cout << "[EquipWeapon] rejected unknown itemObjectId=" << itemObjectId
			<< " playerId=" << player->objectInfo->object_id() << endl;
		return;
	}

	if (weaponState->pickedUp)
	{
		cout << "[EquipWeapon] rejected already picked itemObjectId=" << itemObjectId
			<< " playerId=" << player->objectInfo->object_id() << endl;
		return;
	}

	if (player->objectInfo->weapon_type() != Protocol::WEAPON_TYPE_NONE)
	{
		cout << "[EquipWeapon] rejected because player already has weapon. playerId="
			<< player->objectInfo->object_id()
			<< " currentWeaponType=" << player->objectInfo->weapon_type()
			<< " itemObjectId=" << itemObjectId << endl;
		return;
	}

	weaponState->pickedUp = true;
	player->objectInfo->set_weapon_type(weaponState->weaponType);

	// 다른 사람들에게 뿌릴 S_EQUIP_WEAPON 패킷 조립
	Protocol::S_EQUIP_WEAPON equipPkt;
	equipPkt.set_playerid(player->objectInfo->object_id()); // 누가 주웠는지 (본인)
	equipPkt.set_itemobjectid(itemObjectId);                // 어떤 아이템을 주웠는지
	equipPkt.set_weapontype(weaponState->weaponType);       // 서버에 등록된 무기 타입

	// 방에 있는 모든 사람에게 소문내기 (Broadcast)
	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(equipPkt);
	Broadcast(sendBuffer);

	cout << "[EquipWeapon] playerId=" << player->objectInfo->object_id()
		<< " itemObjectId=" << itemObjectId
		<< " weaponType=" << weaponState->weaponType << endl;
}

void Room::HandlePickupLootItem(PlayerRef player, Protocol::C_PICKUP_LOOT_ITEM pkt)
{
	const uint64 itemId = pkt.item_object_id();
	const uint64 playerId = (player ? player->objectInfo->object_id() : 0);
	cout << "[PickupLootItem] playerId=" << playerId
		<< " itemId=" << itemId
		<< " shouldRespawn=" << pkt.should_respawn()
		<< " respawnDelay=" << pkt.respawn_delay()
		<< endl;

	if (itemId == 0)
	{
		cout << "[PickupLootItem] ignored because itemId is 0" << endl;
		return;
	}

	if (_inactiveLootItemIds.find(itemId) != _inactiveLootItemIds.end())
	{
		cout << "[PickupLootItem] ignored because itemId is already inactive: " << itemId << endl;
		return;
	}

	_inactiveLootItemIds.insert(itemId);

	Protocol::S_DESPAWN despawnPkt;
	Protocol::DespawnInfo* despawnInfo = despawnPkt.add_despawn_infos();
	despawnInfo->set_object_id(itemId);
	despawnInfo->set_object_type(Protocol::OBJECT_TYPE_ITEM);
	cout << "[PickupLootItem] broadcasting despawn for itemId=" << itemId << endl;
	SendBufferRef despawnBuffer = ServerPacketHandler::MakeSendBuffer(despawnPkt);
	Broadcast(despawnBuffer);

	if (pkt.should_respawn() && pkt.respawn_delay() > 0.0f)
	{
		PendingLootItemRespawn& pendingRespawn = _pendingLootItemRespawns.emplace_back();
		pendingRespawn.itemId = itemId;
		pendingRespawn.remainingTime = pkt.respawn_delay();
	}
}

void Room::HandleFire(PlayerRef player, Protocol::C_FIRE pkt)
{
	if (player == nullptr) return;

	Protocol::S_FIRE broadcastPkt;
	broadcastPkt.set_object_id(player->objectInfo->object_id());

	// 방에 있는 모두에게 쐈다고 알림
	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(broadcastPkt);
	Broadcast(sendBuffer);

	cout << "[Server] " << player->objectInfo->object_id() << "번 유저 총기 발사! 남들에게 브로드캐스트 완료." << endl;
}

void Room::HandleEnterTruck(PlayerRef player, Protocol::C_ENTER_TRUCK pkt)
{
	if (player == nullptr)
		return;

	const uint64 playerId = player->objectInfo->object_id();
	const uint64 truckId = pkt.truck_id();
	const Protocol::TruckSeatType seatType = pkt.seat_type();

	if (truckId == 0 || seatType == Protocol::TRUCK_SEAT_NONE)
		return;

	TruckState& truckState = GetOrCreateTruckState(truckId);

	//[신우] cargo <-> turret 전환도 클라이언트에서는 "탑승" 흐름으로 처리하므로,
	//[신우] 서버에서는 좌석 변경이 일어나면 S_ENTER_TRUCK를 다시 브로드캐스트해서 상태를 동기화한다.
	auto BroadcastSeatChange = [&](Protocol::TruckSeatType NewSeatType)
		{
			Protocol::S_ENTER_TRUCK enterPkt;
			enterPkt.set_player_id(playerId);
			enterPkt.set_truck_id(truckId);
			enterPkt.set_seat_type(NewSeatType);

			SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(enterPkt);
			Broadcast(sendBuffer);
		};

	if (player->bIsInTruck)
	{
		if (player->currentTruckId != truckId)
			return;

		if (player->currentTruckSeatType == seatType)
			return;

		const bool bCargoToTurret =
			player->currentTruckSeatType == Protocol::TRUCK_SEAT_CARGO &&
			seatType == Protocol::TRUCK_SEAT_TURRET;
		const bool bTurretToCargo =
			player->currentTruckSeatType == Protocol::TRUCK_SEAT_TURRET &&
			seatType == Protocol::TRUCK_SEAT_CARGO;

		//[신우] 이미 트럭에 타고 있는 상태에서는 cargo <-> turret 전환만 허용한다.
		//[신우] 다른 좌석 변경까지 허용하면 운전석/적재함/기관총 상태가 꼬이기 쉬워서 서버에서 막아둔다.
		if (bCargoToTurret == false && bTurretToCargo == false)
			return;

		if (IsTruckSeatOccupied(truckState, seatType))
			return;

		ClearTruckSeatOccupant(truckState, player->currentTruckSeatType, playerId);
		SetTruckSeatOccupant(truckState, seatType, playerId);
		player->currentTruckSeatType = seatType;
		BroadcastSeatChange(seatType);
		return;
	}

	//[신우] 기관총은 반드시 트럭 내부(cargo)에서만 갈아탈 수 있게 한다.
	//[신우] 트럭 밖에서 바로 기관총에 타는 문제를 서버 권한으로 차단하는 부분이다.
	if (seatType == Protocol::TRUCK_SEAT_TURRET)
		return;

	if (IsTruckSeatOccupied(truckState, seatType))
		return;

	SetTruckSeatOccupant(truckState, seatType, playerId);
	player->bIsInTruck = true;
	player->currentTruckId = truckId;
	player->currentTruckSeatType = seatType;

	// A new driver must start from the last server-approved truck pose, not from
	// the player's standing position beside the truck.
	if (seatType == Protocol::TRUCK_SEAT_DRIVER && truckState.hasTransform)
		BroadcastTruckState(truckState, true);

	BroadcastSeatChange(seatType);
}

void Room::HandleExitTruck(PlayerRef player, Protocol::C_EXIT_TRUCK pkt)
{
	UNREFERENCED_PARAMETER(pkt);

	if (player == nullptr || player->bIsInTruck == false)
		return;

	const uint64 playerId = player->objectInfo->object_id();
	const uint64 truckId = player->currentTruckId;
	const Protocol::TruckSeatType seatType = player->currentTruckSeatType;

	TruckState* truckState = FindTruckState(truckId);
	if (truckState == nullptr)
	{
		ClearPlayerTruckState(player);
		return;
	}

	ClearTruckSeatOccupant(*truckState, seatType, playerId);
	ClearPlayerTruckState(player);

	Protocol::S_EXIT_TRUCK exitPkt;
	exitPkt.set_player_id(playerId);
	exitPkt.set_truck_id(truckId);
	exitPkt.set_seat_type(seatType);

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(exitPkt);
	Broadcast(sendBuffer);
}

void Room::HandleTruckMove(PlayerRef player, Protocol::C_TRUCK_MOVE pkt)
{
	if (player == nullptr || player->bIsInTruck == false)
		return;

	const uint64 truckId = player->currentTruckId;
	TruckState* truckState = FindTruckState(truckId);
	if (truckState == nullptr)
		return;

	const Protocol::PosInfo& incoming = pkt.info();
	if (incoming.object_id() != 0 && incoming.object_id() != truckId)
		return;

	if (std::isfinite(pkt.fuel()) && pkt.fuel() >= 0.0f)
		truckState->fuel = pkt.fuel();

	const uint64 playerId = player->objectInfo->object_id();
	const bool bIsTurretUser =
		player->currentTruckSeatType == Protocol::TRUCK_SEAT_TURRET &&
		truckState->turretPlayerId == playerId;
	if (bIsTurretUser &&
		pkt.has_turret_aim() &&
		std::isfinite(pkt.turret_yaw()) &&
		std::isfinite(pkt.turret_pitch()))
	{
		truckState->hasTurretAim = true;
		truckState->turretYaw = pkt.turret_yaw();
		truckState->turretPitch = pkt.turret_pitch();
	}

	const bool bIsDriver = player->currentTruckSeatType == Protocol::TRUCK_SEAT_DRIVER;
	if (bIsDriver == false)
	{
		BroadcastTruckState(*truckState);
		return;
	}

	if (truckState->driverPlayerId != player->objectInfo->object_id())
		return;

	const bool bFiniteTransform =
		std::isfinite(incoming.x()) &&
		std::isfinite(incoming.y()) &&
		std::isfinite(incoming.z()) &&
		std::isfinite(incoming.yaw()) &&
		std::isfinite(incoming.pitch()) &&
		std::isfinite(incoming.roll());
	if (bFiniteTransform == false)
	{
		BroadcastTruckState(*truckState, true);
		return;
	}

	constexpr float MAX_WORLD_COORDINATE = 5000000.0f;
	if (std::abs(incoming.x()) > MAX_WORLD_COORDINATE ||
		std::abs(incoming.y()) > MAX_WORLD_COORDINATE ||
		std::abs(incoming.z()) > MAX_WORLD_COORDINATE)
	{
		BroadcastTruckState(*truckState, true);
		return;
	}

	if (truckState->hasTransform)
	{
		const float dx = incoming.x() - truckState->posInfo.x();
		const float dy = incoming.y() - truckState->posInfo.y();
		const float dz = incoming.z() - truckState->posInfo.z();
		constexpr float MAX_TRUCK_DELTA_PER_PACKET = 5000.0f;
		if ((dx * dx + dy * dy + dz * dz) >
			MAX_TRUCK_DELTA_PER_PACKET * MAX_TRUCK_DELTA_PER_PACKET)
		{
			BroadcastTruckState(*truckState, true);
			return;
		}
	}

	truckState->posInfo.CopyFrom(incoming);
	truckState->posInfo.set_object_id(truckId);
	truckState->hasTransform = true;

	// The driver character is unpossessed while driving, so C_MOVE stops.
	// Keep the server-side player position on the truck for zombie targeting.
	player->posInfo->set_x(incoming.x());
	player->posInfo->set_y(incoming.y());
	player->posInfo->set_z(incoming.z());
	player->posInfo->set_yaw(incoming.yaw());
	player->posInfo->set_state(incoming.state());

	BroadcastTruckState(*truckState);
}

void Room::HandleLoadTruckItem(PlayerRef player, Protocol::C_LOAD_TRUCK_ITEM pkt)
{
	if (player == nullptr)
		return;

	const uint64 truckId = pkt.truck_id();
	if (truckId == 0 || pkt.item_types_size() <= 0)
		return;

	GetOrCreateTruckState(truckId);

	Protocol::S_LOAD_TRUCK_ITEM loadPkt;
	loadPkt.set_player_id(player->objectInfo->object_id());
	loadPkt.set_truck_id(truckId);
	for (const int32 itemType : pkt.item_types())
	{
		loadPkt.add_item_types(itemType);
	}

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(loadPkt);
	Broadcast(sendBuffer, player->objectInfo->object_id());
}

void Room::HandleToggleDoor(PlayerRef player, Protocol::C_TOGGLE_DOOR pkt)
{
	UNREFERENCED_PARAMETER(player);

	const uint64 doorId = pkt.door_id();
	if (doorId == 0)
		return;

	bool& bIsOpen = _doors[doorId];
	bIsOpen = !bIsOpen;

	Protocol::S_TOGGLE_DOOR doorPkt;
	doorPkt.set_door_id(doorId);
	doorPkt.set_is_open(bIsOpen);

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(doorPkt);
	Broadcast(sendBuffer);
}

void Room::HandleStageTransitionRequest(PlayerRef player, Protocol::C_STAGE_TRANSITION_REQUEST pkt)
{
	if (player == nullptr || _bStageTransitionStarted)
		return;

	const uint64 playerId = player->objectInfo->object_id();
	const uint64 truckId = pkt.truck_id();
	if (truckId == 0 || pkt.target_level().empty())
		return;

	TruckState* truckState = FindTruckState(truckId);
	if (truckState == nullptr)
		return;

	if (truckState->driverPlayerId != playerId)
		return;

	if (_bTruckLoadingPhaseActive && GetTruckLoadingPhaseRemainingSeconds() > 0)
		return;

	if (GetTruckOccupantCount(*truckState) < REQUIRED_STAGE2_PLAYER_COUNT)
		return;

	_bStageTransitionStarted = true;
	_stageTransitionReadyPlayerIds.clear();

	Protocol::S_STAGE_TRANSITION transitionPkt;
	transitionPkt.set_target_level(pkt.target_level());

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(transitionPkt);
	Broadcast(sendBuffer);
}

void Room::UpdateTick()
{
	UpdateZombies();
	BroadcastStageTimer();

	for (int32 index = static_cast<int32>(_pendingZombieDespawns.size()) - 1; index >= 0; --index)
	{
		PendingZombieDespawn& pending = _pendingZombieDespawns[index];
		pending.remainingTime -= ZOMBIE_SERVER_TICK_SECONDS;
		if (pending.remainingTime > 0.0f)
			continue;

		if (_objects.find(pending.zombieId) != _objects.end())
		{
			Protocol::S_DESPAWN despawnPkt;
			Protocol::DespawnInfo* despawnInfo = despawnPkt.add_despawn_infos();
			despawnInfo->set_object_id(pending.zombieId);
			despawnInfo->set_object_type(Protocol::OBJECT_TYPE_CREATURE);

			RemoveObject(pending.zombieId);

			SendBufferRef despawnBuffer = ServerPacketHandler::MakeSendBuffer(despawnPkt);
			Broadcast(despawnBuffer);
		}

		_pendingZombieDespawns.erase(_pendingZombieDespawns.begin() + index);
	}

	for (int32 index = static_cast<int32>(_pendingLootItemRespawns.size()) - 1; index >= 0; --index)
	{
		PendingLootItemRespawn& pending = _pendingLootItemRespawns[index];
		pending.remainingTime -= ZOMBIE_SERVER_TICK_SECONDS;
		if (pending.remainingTime > 0.0f)
			continue;

		_inactiveLootItemIds.erase(pending.itemId);

		Protocol::S_RESPAWN_LOOT_ITEM respawnPkt;
		respawnPkt.add_item_object_ids(pending.itemId);
		SendBufferRef respawnBuffer = ServerPacketHandler::MakeSendBuffer(respawnPkt);
		Broadcast(respawnBuffer);

		_pendingLootItemRespawns.erase(_pendingLootItemRespawns.begin() + index);
	}

	DoTimer(100, &Room::UpdateTick);
}

RoomRef Room::GetRoomRef()
{
	return static_pointer_cast<Room>(shared_from_this());
}

bool Room::AddObject(ObjectRef object)
{
	// 있다면 문제가 있다.
	if (_objects.find(object->objectInfo->object_id()) != _objects.end())
		return false;

	_objects.insert(make_pair(object->objectInfo->object_id(), object));

	object->room.store(GetRoomRef());

	return true;
}

bool Room::RemoveObject(uint64 objectId)
{
	// 없다면 문제가 있다.
	if (_objects.find(objectId) == _objects.end())
		return false;

	ObjectRef object = _objects[objectId];
	PlayerRef player = dynamic_pointer_cast<Player>(object);
	if (player)
	{
		player->room.store(weak_ptr<Room>());
		ClearPlayerTruckState(player);
	}

	if (objectId >= ZOMBIE_OBJECT_ID_START)
		_zombiePaths.erase(objectId);

	_objects.erase(objectId);

	return true;
}

void Room::Broadcast(SendBufferRef sendBuffer, uint64 exceptId)
{
	for (auto& item : _objects)
	{
		PlayerRef player = dynamic_pointer_cast<Player>(item.second);
		if (player == nullptr)
			continue;
		if (player->objectInfo->object_id() == exceptId)
			continue;

		if (GameSessionRef session = player->session.lock())
			session->Send(sendBuffer);
	}
}

void Room::BroadcastPendingReadyCount()
{
	vector<weak_ptr<GameSession>> CleanedPendingSessions;
	CleanedPendingSessions.reserve(_pendingReadySessions.size());

	for (const weak_ptr<GameSession>& PendingSessionWeak : _pendingReadySessions)
	{
		GameSessionRef PendingSession = PendingSessionWeak.lock();
		if (PendingSession == nullptr || PendingSession->player.load() != nullptr)
		{
			continue;
		}

		CleanedPendingSessions.push_back(PendingSession);
	}

	_pendingReadySessions.swap(CleanedPendingSessions);

	Protocol::S_ENTER_GAME_READY_COUNT readyCountPkt;
	readyCountPkt.set_ready_count(static_cast<int32>(_pendingReadySessions.size()));
	readyCountPkt.set_required_count(static_cast<int32>(REQUIRED_STAGE2_PLAYER_COUNT));

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(readyCountPkt);
	for (const weak_ptr<GameSession>& PendingSessionWeak : _pendingReadySessions)
	{
		if (GameSessionRef PendingSession = PendingSessionWeak.lock())
		{
			PendingSession->Send(sendBuffer);
		}
	}
}

void Room::BroadcastStageTransitionReadyCount()
{
	Protocol::S_ENTER_GAME_READY_COUNT readyCountPkt;
	readyCountPkt.set_ready_count(static_cast<int32>(_stageTransitionReadyPlayerIds.size()));
	readyCountPkt.set_required_count(static_cast<int32>(REQUIRED_STAGE2_PLAYER_COUNT));

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(readyCountPkt);
	Broadcast(sendBuffer);
}

void Room::StartTruckLoadingPhase()
{
	_bTruckLoadingPhaseActive = true;
	_bStageTransitionStarted = false;
	_stageTransitionReadyPlayerIds.clear();
	_truckLoadingPhaseEndTime = std::chrono::steady_clock::now() + std::chrono::seconds(TRUCK_LOADING_PHASE_DURATION_SECONDS);
	_lastBroadcastTruckLoadingRemainingSeconds = -1;
	BroadcastStageTimer();
}

void Room::BroadcastStageTimer()
{
	if (_bTruckLoadingPhaseActive == false)
		return;

	const int32 remainingSeconds = GetTruckLoadingPhaseRemainingSeconds();
	if (remainingSeconds == _lastBroadcastTruckLoadingRemainingSeconds)
		return;

	Protocol::S_STAGE_TIMER timerPkt;
	timerPkt.set_remaining_seconds(remainingSeconds);
	timerPkt.set_is_loading_phase(remainingSeconds > 0);

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(timerPkt);
	Broadcast(sendBuffer);

	_lastBroadcastTruckLoadingRemainingSeconds = remainingSeconds;
	if (remainingSeconds <= 0)
		_bTruckLoadingPhaseActive = false;
}

void Room::SendStageTimerToSession(const GameSessionRef& session) const
{
	if (session == nullptr || _bTruckLoadingPhaseActive == false)
		return;

	Protocol::S_STAGE_TIMER timerPkt;
	timerPkt.set_remaining_seconds(GetTruckLoadingPhaseRemainingSeconds());
	timerPkt.set_is_loading_phase(true);

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(timerPkt);
	session->Send(sendBuffer);
}

int32 Room::GetTruckLoadingPhaseRemainingSeconds() const
{
	if (_bTruckLoadingPhaseActive == false)
		return 0;

	const auto now = std::chrono::steady_clock::now();
	if (now >= _truckLoadingPhaseEndTime)
		return 0;

	const int64 remainingMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(_truckLoadingPhaseEndTime - now).count();
	return static_cast<int32>((remainingMilliseconds + 999) / 1000);
}

void Room::SendStage1ItemSeedToSession(const GameSessionRef& session) const
{
	if (session == nullptr || _stage1ItemSpawnSeed == 0)
		return;

	Protocol::S_STAGE1_ITEM_SEED seedPkt;
	seedPkt.set_seed(_stage1ItemSpawnSeed);

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(seedPkt);
	session->Send(sendBuffer);
}