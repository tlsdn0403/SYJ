#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "GameSession.h"
#include "Monster.h"
#include "ObjectUtils.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <limits>

namespace
{
constexpr float TRUCK_STATE_EPSILON = 0.01f;
constexpr int32 MOUNTED_GUN_AMMO_ITEM_TYPE = 5;
constexpr auto TRUCK_ITEM_STALE_PACKET_GRACE = std::chrono::milliseconds(1000);

bool IsRecentTruckItemUpdate(std::chrono::steady_clock::time_point LastUpdateTime)
{
	return LastUpdateTime.time_since_epoch().count() != 0 &&
		std::chrono::steady_clock::now() - LastUpdateTime < TRUCK_ITEM_STALE_PACKET_GRACE;
}
}

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

	ApplyPendingStage2MachineGunAmmo(truckState);
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
	if (truckState.hasFuel)
	{
		movePkt.set_has_truck_fuel(true);
		movePkt.set_fuel(truckState.fuel);
	}
	if (truckState.hasHealth)
	{
		movePkt.set_has_truck_health(true);
		movePkt.set_truck_hp(truckState.hp);
		movePkt.set_truck_max_hp(truckState.maxHp);
	}
	if (truckState.hasTurretAim)
	{
		movePkt.set_has_turret_aim(true);
		movePkt.set_turret_yaw(truckState.turretYaw);
		movePkt.set_turret_pitch(truckState.turretPitch);
	}
	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(movePkt);
	Broadcast(sendBuffer);
}

void Room::RefreshMachineGunAmmoFromCargo(TruckState& truckState)
{
	const int32 safeMaxAmmo = (std::max)(truckState.machineGunMaxAmmo, 0);
	const int32 safeMountedAmmoCount = (std::max)(truckState.mountedAmmoCount, 0);
	const int32 ammoCountDelta = safeMountedAmmoCount - truckState.lastSyncedMountedAmmoCount;

	if (ammoCountDelta != 0)
	{
		truckState.machineGunTotalAmmo = (std::max)(truckState.machineGunTotalAmmo + (ammoCountDelta * safeMaxAmmo), 0);
		truckState.lastSyncedMountedAmmoCount = safeMountedAmmoCount;
	}
	else if (truckState.lastSyncedMountedAmmoCount == 0 && truckState.machineGunTotalAmmo == 0 && safeMountedAmmoCount > 0)
	{
		truckState.machineGunTotalAmmo = safeMountedAmmoCount * safeMaxAmmo;
		truckState.lastSyncedMountedAmmoCount = safeMountedAmmoCount;
	}

	truckState.machineGunCurrentAmmo = (std::min)((std::max)(truckState.machineGunCurrentAmmo, 0), safeMaxAmmo);
	if (truckState.machineGunCurrentAmmo <= 0 && truckState.machineGunTotalAmmo > 0)
	{
		truckState.machineGunCurrentAmmo = (std::min)(safeMaxAmmo, truckState.machineGunTotalAmmo);
	}
}

bool Room::ConsumeMachineGunBullet(TruckState& truckState)
{
	RefreshMachineGunAmmoFromCargo(truckState);

	if (truckState.machineGunTotalAmmo <= 0 || truckState.machineGunCurrentAmmo <= 0)
	{
		truckState.machineGunTotalAmmo = (std::max)(truckState.machineGunTotalAmmo, 0);
		truckState.machineGunCurrentAmmo = 0;
		return false;
	}

	--truckState.machineGunTotalAmmo;
	--truckState.machineGunCurrentAmmo;

	if (truckState.machineGunCurrentAmmo <= 0 && truckState.machineGunTotalAmmo > 0)
	{
		truckState.machineGunCurrentAmmo = (std::min)((std::max)(truckState.machineGunMaxAmmo, 0), truckState.machineGunTotalAmmo);
	}

	return true;
}

void Room::BroadcastMachineGunAmmo(const TruckState& truckState)
{
	const uint64 truckId = truckState.posInfo.object_id();
	if (truckId == 0)
	{
		return;
	}

	Protocol::S_MACHINE_GUN_AMMO ammoPkt;
	ammoPkt.set_truck_id(truckId);
	ammoPkt.set_total_ammo((std::max)(truckState.machineGunTotalAmmo, 0));
	ammoPkt.set_current_ammo((std::max)(truckState.machineGunCurrentAmmo, 0));
	ammoPkt.set_max_ammo((std::max)(truckState.machineGunMaxAmmo, 0));

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(ammoPkt);
	Broadcast(sendBuffer);
}

void Room::CaptureStage2MachineGunAmmoFromTruck(uint64 truckId)
{
	_bHasPendingStage2MachineGunAmmo = false;
	_pendingStage2MountedAmmoCount = 0;
	_pendingStage2MachineGunMaxAmmo = 100;
	_pendingStage2MachineGunTotalAmmo = 0;
	_pendingStage2MachineGunCurrentAmmo = 0;

	TruckState* truckState = FindTruckState(truckId);
	if (truckState == nullptr)
		return;

	RefreshMachineGunAmmoFromCargo(*truckState);

	_pendingStage2MountedAmmoCount = (std::max)(truckState->mountedAmmoCount, 0);
	_pendingStage2MachineGunMaxAmmo = (std::max)(truckState->machineGunMaxAmmo, 0);
	_pendingStage2MachineGunTotalAmmo = (std::max)(truckState->machineGunTotalAmmo, 0);
	_pendingStage2MachineGunCurrentAmmo = (std::min)((std::max)(truckState->machineGunCurrentAmmo, 0), _pendingStage2MachineGunMaxAmmo);
	_bHasPendingStage2MachineGunAmmo =
		_pendingStage2MountedAmmoCount > 0 ||
		_pendingStage2MachineGunTotalAmmo > 0 ||
		_pendingStage2MachineGunCurrentAmmo > 0;

	cout << "[Stage2MachineGunAmmo] Captured truckId=" << truckId
		<< " mountedAmmoCount=" << _pendingStage2MountedAmmoCount
		<< " total=" << _pendingStage2MachineGunTotalAmmo
		<< " current=" << _pendingStage2MachineGunCurrentAmmo
		<< " max=" << _pendingStage2MachineGunMaxAmmo << endl;
}

bool Room::ApplyPendingStage2MachineGunAmmo(TruckState& truckState)
{
	if (_bHasPendingStage2MachineGunAmmo == false)
		return false;

	if (truckState.posInfo.object_id() == 0)
		return false;

	truckState.mountedAmmoCount = _pendingStage2MountedAmmoCount;
	truckState.lastSyncedMountedAmmoCount = _pendingStage2MountedAmmoCount;
	truckState.machineGunMaxAmmo = _pendingStage2MachineGunMaxAmmo;
	truckState.machineGunTotalAmmo = _pendingStage2MachineGunTotalAmmo;
	truckState.machineGunCurrentAmmo = _pendingStage2MachineGunCurrentAmmo;

	_bHasPendingStage2MachineGunAmmo = false;
	_pendingStage2MountedAmmoCount = 0;
	_pendingStage2MachineGunTotalAmmo = 0;
	_pendingStage2MachineGunCurrentAmmo = 0;

	cout << "[Stage2MachineGunAmmo] Applied truckId=" << truckState.posInfo.object_id()
		<< " mountedAmmoCount=" << truckState.mountedAmmoCount
		<< " total=" << truckState.machineGunTotalAmmo
		<< " current=" << truckState.machineGunCurrentAmmo
		<< " max=" << truckState.machineGunMaxAmmo << endl;

	BroadcastMachineGunAmmo(truckState);
	return true;
}
bool Room::EnterRoom(ObjectRef object, bool randPos /*= true*/)
{
	bool success = AddObject(object);
	if (success == false)
	{
		if (auto player = dynamic_pointer_cast<Player>(object))
		{
			Protocol::S_ENTER_GAME enterGamePkt;
			enterGamePkt.set_success(false);

			if (object != nullptr && object->objectInfo != nullptr)
			{
				Protocol::ObjectInfo* playerInfo = new Protocol::ObjectInfo();
				playerInfo->CopyFrom(*object->objectInfo);
				enterGamePkt.set_allocated_player(playerInfo);
			}

			SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(enterGamePkt);
			if (auto session = player->session.lock())
				session->Send(sendBuffer);
		}

		return false;
	}

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

		for (const auto& item : _players)
		{
			if (item.second == object)
				continue;

			Protocol::ObjectInfo* playerInfo = spawnPkt.add_players();
			playerInfo->CopyFrom(*item.second->objectInfo);
		}

		for (const auto& item : _monsters)
		{
			if (item.second == object)
				continue;

			Protocol::ObjectInfo* monsterInfo = spawnPkt.add_players();
			monsterInfo->CopyFrom(*item.second->objectInfo);
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
	{
		if (player)
		{
			if (auto session = player->session.lock())
				session->player.store(nullptr);
			player->session.reset();
		}
		return false;
	}

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

	GameSessionRef session = player->session.lock();

	ForceExitTruck(player);

	// 나가는 유저의 ID를 미리 기억해둠
	uint64 leaveId = player->objectInfo->object_id();

	// 기존에 만들어둔 방 퇴장 로직 실행 (서버 내부 장부에서 지우는 역할)
	bool success = LeaveRoom(player);

	// 퇴장에 실패했거나 이미 나간 유저라면 여기서 끝냄
	if (success == false) return false;

	if (session)
		session->player.store(nullptr);
	player->session.reset();

	// 방에 남아있는 다른 사람들에게 "얘 나갔다"고 소문내기!
	Protocol::S_LEAVE_GAME leavePkt;
	leavePkt.set_object_id(leaveId);

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(leavePkt);
	Broadcast(sendBuffer);

	return true;
}

void Room::HandleReadyPlayer(GameSessionRef session, Protocol::C_ENTER_GAME pkt)
{
	RecordStage2TileSequence(pkt);

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

void Room::HandleStageMapReady(GameSessionRef session, Protocol::C_ENTER_GAME pkt)
{
	RecordStage2TileSequence(pkt);

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
	for (const auto& item : _players)
	{
		PlayerRef player = item.second;
		if (player == nullptr)
			continue;

		const uint64 playerId = player->objectInfo->object_id();
		if (_stageTransitionReadyPlayerIds.find(playerId) == _stageTransitionReadyPlayerIds.end())
			continue;

		readyPlayers.push_back(player);
	}

	CaptureStage2MachineGunAmmoFromTruck(_stageTransitionTruckId);
	TruckState stage2TruckState;
	bool bHasStage2TruckState = false;
	if (TruckState* transitionTruckState = FindTruckState(_stageTransitionTruckId))
	{
		stage2TruckState = *transitionTruckState;
		bHasStage2TruckState = true;
	}

	for (const PlayerRef& player : readyPlayers)
	{
		ClearPlayerTruckState(player);
	}
	_trucks.clear();
	if (bHasStage2TruckState)
	{
		stage2TruckState.driverPlayerId = 0;
		stage2TruckState.cargoPlayerIds.clear();
		stage2TruckState.turretPlayerId = 0;
		stage2TruckState.hasTurretAim = false;
		stage2TruckState.posInfo.Clear();
		stage2TruckState.posInfo.set_object_id(_stageTransitionTruckId);
		stage2TruckState.posInfo.set_x(-3065.368f);
		stage2TruckState.posInfo.set_y(15748.147f);
		stage2TruckState.posInfo.set_z(100.0f);
		stage2TruckState.posInfo.set_yaw(-90.0f);
		stage2TruckState.posInfo.set_pitch(0.0f);
		stage2TruckState.posInfo.set_roll(0.0f);
		stage2TruckState.posInfo.set_state(Protocol::MOVE_STATE_IDLE);
		stage2TruckState.hasTransform = true;
		stage2TruckState.hasFuel = true;
		stage2TruckState.fuel = 100.0f;
		_trucks.emplace(_stageTransitionTruckId, std::move(stage2TruckState));
	}
	const uint64 stage2TruckId = _stageTransitionTruckId;
	_stageTransitionTruckId = 0;

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

	if (TruckState* truckState = FindTruckState(stage2TruckId))
	{
		BroadcastTruckState(*truckState, true);
		BroadcastMachineGunAmmo(*truckState);
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
	constexpr float ZOMBIE_MOVE_SPEED = 250.0f;
	constexpr float ZOMBIE_AGGRO_RANGE = 8000.0f;
	constexpr float ZOMBIE_AI_ACTIVE_RANGE = 8200.0f;
	constexpr float ZOMBIE_ATTACK_RANGE = 140.0f;
	constexpr float ZOMBIE_ATTACK_COOLDOWN_SECONDS = 1.0f;
	constexpr float ZOMBIE_DESPAWN_DELAY_SECONDS = 3.0f;
	constexpr float ZOMBIE_SEPARATION_RADIUS = 180.0f;
	constexpr float ZOMBIE_SEPARATION_GRID_CELL_SIZE = ZOMBIE_SEPARATION_RADIUS;
	constexpr float ZOMBIE_SEPARATION_WEIGHT = 1.35f;
	constexpr int32 ZOMBIE_SEPARATION_MAX_NEIGHBORS = 8;
	constexpr float ZOMBIE_YAW_TURN_RATE_DEGREES = 360.0f;
	constexpr float ZOMBIE_TRUCK_BODY_HALF_LENGTH = 270.0f;
	constexpr float ZOMBIE_TRUCK_BODY_HALF_WIDTH = 130.0f;
	constexpr float ZOMBIE_TRUCK_TARGET_STANDOFF = 85.0f;
	constexpr float ZOMBIE_TRUCK_ATTACK_RANGE = 45.0f;
	constexpr float ZOMBIE_TRUCK_IMPACT_EVENT_MIN_DAMAGE = 35.0f;
	constexpr float ZOMBIE_TRUCK_IMPACT_BASE_DAMAGE = 50.0f;
	constexpr float ZOMBIE_TRUCK_IMPACT_FATAL_DAMAGE = 200.0f;
	constexpr float ZOMBIE_TRUCK_IMPACT_BASE_IMPULSE = 260000.0f;
	constexpr float ZOMBIE_PATH_RECALC_SECONDS = 0.75f;
	constexpr float ZOMBIE_PATH_TARGET_REPATH_DISTANCE = 300.0f;
	constexpr float ZOMBIE_WAYPOINT_REACHED_DISTANCE = 80.0f;
	constexpr float ZOMBIE_NAV_GRID_CELL_SIZE = 300.0f;
	constexpr int32 ZOMBIE_NAV_MAX_SEARCH_NODES = 512;
	constexpr bool ZOMBIE_NAV_HAS_BLOCKED_CELLS = false;
	constexpr int32 STAGE2_ZOMBIES_TO_SPAWN_PER_TICK = 10;
	constexpr int32 STAGE2_ZOMBIE_GROUP_COLUMNS = 4;
	constexpr int32 STAGE2_ZOMBIE_GROUP_ROWS = 5;
	constexpr float ZOMBIE_AI_NEAR_RANGE = 1800.0f;
	constexpr float ZOMBIE_AI_MID_RANGE = 3600.0f;
	constexpr float ZOMBIE_AI_NEAR_UPDATE_INTERVAL = ZOMBIE_SERVER_TICK_SECONDS;
	constexpr float ZOMBIE_AI_MID_UPDATE_INTERVAL = 0.2f;
	constexpr float ZOMBIE_AI_FAR_UPDATE_INTERVAL = 0.4f;
	constexpr float ZOMBIE_MOVE_BROADCAST_INTERVAL = 0.30f;
	constexpr float ZOMBIE_MOVE_BROADCAST_DISTANCE = 90.0f;
	constexpr float ZOMBIE_MOVE_BROADCAST_YAW_DELTA = 25.0f;
	constexpr int32 STAGE2_ZOMBIE_TILE_WORLD = 0;
	constexpr int32 STAGE2_ZOMBIE_TILE_STRAIGHT = 1;
	constexpr int32 STAGE2_ZOMBIE_TILE_LEFT = 2;
	constexpr int32 STAGE2_ZOMBIE_TILE_RIGHT = 3;
	constexpr int32 STAGE2_ZOMBIE_TILE_START = 4;
	constexpr int32 STAGE2_START_TILE_OCCURRENCE_COUNT = 1;
	constexpr int32 STAGE2_STRAIGHT_TILE_OCCURRENCE_COUNT = 3;
	constexpr int32 STAGE2_LEFT_TILE_OCCURRENCE_COUNT = 2;
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

	float NormalizeYawDegrees(float yaw)
	{
		if (!std::isfinite(yaw))
			return 0.0f;

		yaw = fmodf(yaw + 180.0f, 360.0f);
		if (yaw < 0.0f)
			yaw += 360.0f;

		return yaw - 180.0f;
	}

	float FindDeltaYawDegrees(float fromYaw, float toYaw)
	{
		return NormalizeYawDegrees(toYaw - fromYaw);
	}

	float StepYawTowards(float currentYaw, float desiredYaw, float maxStepDegrees)
	{
		const float yawDelta = FindDeltaYawDegrees(currentYaw, desiredYaw);
		const float clampedStep = maxStepDegrees > 0.0f ? maxStepDegrees : 0.0f;
		if (fabsf(yawDelta) <= clampedStep)
			return NormalizeYawDegrees(desiredYaw);

		return NormalizeYawDegrees(currentYaw + (yawDelta > 0.0f ? clampedStep : -clampedStep));
	}

	float ClampFloat(float value, float minValue, float maxValue)
	{
		if (value < minValue)
			return minValue;

		if (value > maxValue)
			return maxValue;

		return value;
	}

	void GetTruckZombieApproachPoint(const Protocol::PosInfo& truckPos, const Protocol::PosInfo& zombiePos, float& outX, float& outY)
	{
		const float yawRadians = NormalizeYawDegrees(truckPos.yaw()) * (3.1415926535f / 180.0f);
		const float forwardX = cosf(yawRadians);
		const float forwardY = sinf(yawRadians);
		const float rightX = -sinf(yawRadians);
		const float rightY = cosf(yawRadians);

		const float relX = zombiePos.x() - truckPos.x();
		const float relY = zombiePos.y() - truckPos.y();
		const float localX = relX * forwardX + relY * forwardY;
		const float localY = relX * rightX + relY * rightY;
		float surfaceX = ClampFloat(localX, -ZOMBIE_TRUCK_BODY_HALF_LENGTH, ZOMBIE_TRUCK_BODY_HALF_LENGTH);
		float surfaceY = ClampFloat(localY, -ZOMBIE_TRUCK_BODY_HALF_WIDTH, ZOMBIE_TRUCK_BODY_HALF_WIDTH);

		const bool outsideX = fabsf(localX) > ZOMBIE_TRUCK_BODY_HALF_LENGTH;
		const bool outsideY = fabsf(localY) > ZOMBIE_TRUCK_BODY_HALF_WIDTH;
		float normalX = 0.0f;
		float normalY = 0.0f;

		if (outsideX || outsideY)
		{
			normalX = localX - surfaceX;
			normalY = localY - surfaceY;
			const float normalLenSq = normalX * normalX + normalY * normalY;
			if (normalLenSq > 0.001f)
			{
				const float normalLen = sqrtf(normalLenSq);
				normalX /= normalLen;
				normalY /= normalLen;
			}
		}

		if (normalX * normalX + normalY * normalY <= 0.001f)
		{
			const float distanceToXFace = ZOMBIE_TRUCK_BODY_HALF_LENGTH - fabsf(localX);
			const float distanceToYFace = ZOMBIE_TRUCK_BODY_HALF_WIDTH - fabsf(localY);
			if (distanceToXFace < distanceToYFace)
			{
				const float sign = localX >= 0.0f ? 1.0f : -1.0f;
				normalX = sign;
				normalY = 0.0f;
				surfaceX = sign * ZOMBIE_TRUCK_BODY_HALF_LENGTH;
			}
			else
			{
				const float sign = localY >= 0.0f ? 1.0f : -1.0f;
				normalX = 0.0f;
				normalY = sign;
				surfaceY = sign * ZOMBIE_TRUCK_BODY_HALF_WIDTH;
			}
		}

		const float approachLocalX = surfaceX + normalX * ZOMBIE_TRUCK_TARGET_STANDOFF;
		const float approachLocalY = surfaceY + normalY * ZOMBIE_TRUCK_TARGET_STANDOFF;
		outX = truckPos.x() + forwardX * approachLocalX + rightX * approachLocalY;
		outY = truckPos.y() + forwardY * approachLocalX + rightY * approachLocalY;
	}
	ZombieNavCell WorldToZombieNavCell(float x, float y)
	{
		return
		{
			static_cast<int32>(floorf(x / ZOMBIE_NAV_GRID_CELL_SIZE)),
			static_cast<int32>(floorf(y / ZOMBIE_NAV_GRID_CELL_SIZE))
		};
	}

	ZombieNavCell WorldToZombieSeparationCell(float x, float y)
	{
		return
		{
			static_cast<int32>(floorf(x / ZOMBIE_SEPARATION_GRID_CELL_SIZE)),
			static_cast<int32>(floorf(y / ZOMBIE_SEPARATION_GRID_CELL_SIZE))
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

	float GetZombieAiUpdateInterval(float targetDistSq)
	{
		if (targetDistSq <= ZOMBIE_AI_NEAR_RANGE * ZOMBIE_AI_NEAR_RANGE)
			return ZOMBIE_AI_NEAR_UPDATE_INTERVAL;

		if (targetDistSq <= ZOMBIE_AI_MID_RANGE * ZOMBIE_AI_MID_RANGE)
			return ZOMBIE_AI_MID_UPDATE_INTERVAL;

		return ZOMBIE_AI_FAR_UPDATE_INTERVAL;
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

	constexpr float PickStage2ZombieUnitFloat(uint32 seed, int32 index)
	{
		return static_cast<float>(MixStage2ZombieSeed(seed, index) & 0x00FFFFFFu) / 16777215.0f;
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
		float formationYawDegrees = 0.0f;
	};

	constexpr ZombieSpawnGroupInfo STAGE2_ZOMBIE_GROUPS[] =
	{
		{ -1810.0f, 16720.0f, 240.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_START, STAGE2_START_TILE_OCCURRENCE_COUNT, 0x6A09E667u, 0xBB67AE85u },
		{ -2010.0f, 9730.0f, 300.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_START, STAGE2_START_TILE_OCCURRENCE_COUNT, 0x3C6EF372u, 0xA54FF53Au },
		{ -4840.0f, 10210.0f, 310.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_START, STAGE2_START_TILE_OCCURRENCE_COUNT, 0x510E527Fu, 0x9B05688Cu },
		{ -1770.0f, 1060.0f, 170.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_START, STAGE2_START_TILE_OCCURRENCE_COUNT, 0x1F83D9ABu, 0x5BE0CD19u },
		{ -4570.0f, -19510.0f, 240.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_START, STAGE2_START_TILE_OCCURRENCE_COUNT, 0x8C3D37C9u, 0x243F6A88u },
		{ -1810.0f, 16720.0f, 240.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_STRAIGHT, STAGE2_STRAIGHT_TILE_OCCURRENCE_COUNT, 0xA24BAED5u, 0x9FB21C63u },
		{ -2010.0f, 9730.0f, 300.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_STRAIGHT, STAGE2_STRAIGHT_TILE_OCCURRENCE_COUNT, 0xC13FA9A9u, 0x5D588B65u },
		{ -4840.0f, 10210.0f, 310.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_STRAIGHT, STAGE2_STRAIGHT_TILE_OCCURRENCE_COUNT, 0x91E10DA5u, 0xB7E15162u },
		{ -1770.0f, 1060.0f, 170.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_STRAIGHT, STAGE2_STRAIGHT_TILE_OCCURRENCE_COUNT, 0x7F4A7C15u, 0xD1B54A32u },
		{ -4570.0f, -19510.0f, 240.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_STRAIGHT, STAGE2_STRAIGHT_TILE_OCCURRENCE_COUNT, 0x3C6EF372u, 0xBB67AE85u },
		{ 25080.0f, 9820.0f, 110.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_LEFT, STAGE2_LEFT_TILE_OCCURRENCE_COUNT, 0xF00DBA11u, 0x10203040u },
		{ 16840.0f, 10790.0f, 180.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_LEFT, STAGE2_LEFT_TILE_OCCURRENCE_COUNT, 0xC001D00Du, 0x55667788u },
		{ 15590.0f, 2430.0f, 110.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_LEFT, STAGE2_LEFT_TILE_OCCURRENCE_COUNT, 0xA5A5F00Du, 0x89ABCDEFu },
		{ 21750.0f, 5310.0f, 80.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_LEFT, STAGE2_LEFT_TILE_OCCURRENCE_COUNT, 0x1BADB002u, 0x76543210u },
		{ 19730.0, 12150.0f, 310.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_LEFT, STAGE2_LEFT_TILE_OCCURRENCE_COUNT, 0xDEADC0DEu, 0x0F1E2D3Cu },
		{ 4800.0f, 5330.0f, 50.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_LEFT, STAGE2_LEFT_TILE_OCCURRENCE_COUNT, 0x31415926u, 0x27182818u },
		{ 4770.0f, 5360.0f, 50.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_RIGHT, STAGE2_RIGHT_TILE_OCCURRENCE_COUNT, 0x41C64E6Du, 0xA341316Cu },
		{ 15280.0f, 2310.0f, 230.0, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_RIGHT, STAGE2_RIGHT_TILE_OCCURRENCE_COUNT, 0x6D2B79F5u, 0x13A5C89Bu },
		{ 21570.8f, 5466.7f, -367.0f, 2, 4, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_RIGHT, STAGE2_RIGHT_TILE_OCCURRENCE_COUNT, 0x9E3779B9u, 0xC2B2AE35u },
		{ 25180.0f, 10670.0f, 380.0, 2, 4, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_RIGHT, STAGE2_RIGHT_TILE_OCCURRENCE_COUNT, 0x85EBCA6Bu, 0x27D4EB2Fu },
		{ 19390.0f, 12080.0f, 310.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_RIGHT, STAGE2_RIGHT_TILE_OCCURRENCE_COUNT, 0x165667B1u, 0xD3A2646Cu },
		{ 16390.0f, 12830.0f, 260.0f, 2, 4, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_RIGHT, STAGE2_RIGHT_TILE_OCCURRENCE_COUNT, 0x27D4EB2Du, 0x94D049BBu },
	};

	constexpr ZombieSpawnGroupInfo STAGE2_STATIC_MAP_ZOMBIE_GROUPS[] =
	{
		// Cube and Cube2-Cube60 are the 60 hand-placed spawn markers.
		// Every marker uses a 2x5 formation: 10 zombies per marker, 600 total.
		{ -1860.0f, 9690.0f, 300.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x8F1BBCDCu, 0xD1310BA6u },
		{ -4900.0f, 10340.0f, 330.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xA4093822u, 0x299F31D0u },
		{ -1940.0f, 1460.0f, 140.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x082EFA98u, 0xEC4E6C89u },
		{ -1470.0f, 3580.0f, 140.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x452821E6u, 0x38D01377u },
		{ -2470.0f, -2900.0f, 150.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xBE5466CFu, 0x34E90C6Cu },
		{ -2180.0f, -6710.0f, 150.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xC0AC29B7u, 0xC97C50DDu },
		{ -4570.0f, -9670.0f, 130.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x3F84D5B5u, 0xB5470917u },
		{ -1690.0f, -11750.0f, 150.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x9216D5D9u, 0x8979FB1Bu },
		{ -1810.0f, -17410.0f, 150.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x8F1BBCDCu, 0xD1310BA6u },
		{ -2150.0f, -23440.0f, 160.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xA4093822u, 0x299F31D0u },
		{ -2060.0f, -20740.0f, 140.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x082EFA98u, 0xEC4E6C89u },
		{ -4470.0f, -28430.0f, 170.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x452821E6u, 0x38D01377u },
		{ -7890.0f, -34130.0f, 310.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xBE5466CFu, 0x34E90C6Cu },
		{ -5760.0f, -35840.0f, 140.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xC0AC29B7u, 0xC97C50DDu },
		{ -190.0f, -36440.0f, 230.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x3F84D5B5u, 0xB5470917u },
		{ -9840.0f, -40120.0f, 370.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x9216D5D9u, 0x8979FB1Bu },
		{ -8860.0f, -42560.0f, 270.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x8F1BBCDCu, 0xD1310BA6u },
		{ -6610.0f, -39090.0f, 160.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xA4093822u, 0x299F31D0u },
		{ -4910.0f, -42250.0f, 560.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x082EFA98u, 0xEC4E6C89u },
		{ -7310.0f, -46150.0f, 130.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x452821E6u, 0x38D01377u },
		{ -160.0f, -43750.0f, 220.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xBE5466CFu, 0x34E90C6Cu },
		{ -2410.0f, -40070.0f, 440.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xC0AC29B7u, 0xC97C50DDu },
		{ 250.0f, -48870.0f, 240.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x3F84D5B5u, 0xB5470917u },
		{ -3170.0f, -54650.0f, 150.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x9216D5D9u, 0x8979FB1Bu },
		{ -8390.0f, -54840.0f, 140.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x8F1BBCDCu, 0xD1310BA6u },
		{ -14820.0f, -54960.0f, 120.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xA4093822u, 0x299F31D0u },
		{ -17890.0f, -54680.0f, 130.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x082EFA98u, 0xEC4E6C89u },
		{ -24430.0f, -52860.0f, 150.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x452821E6u, 0x38D01377u },
		{ -27610.0f, -55370.0f, 150.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xBE5466CFu, 0x34E90C6Cu },
		{ -30170.0f, -52770.0f, 140.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xC0AC29B7u, 0xC97C50DDu },
		{ -33320.0f, -57590.0f, 160.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x3F84D5B5u, 0xB5470917u },
		{ -33530.0f, -64060.0f, 320.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x9216D5D9u, 0x8979FB1Bu },
		{ -28130.0f, -60750.0f, 760.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x8F1BBCDCu, 0xD1310BA6u },
		{ -24370.0f, -65030.0f, 300.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xA4093822u, 0x299F31D0u },
		{ -26160.0f, -66800.0f, 270.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x082EFA98u, 0xEC4E6C89u },
		{ -30470.0f, -69570.0f, 150.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x452821E6u, 0x38D01377u },
		{ -25440.0f, -73120.0f, 340.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xBE5466CFu, 0x34E90C6Cu },
		{ -32960.0f, -72050.0f, 140.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xC0AC29B7u, 0xC97C50DDu },
		{ -29170.0f, -78210.0f, 140.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x3F84D5B5u, 0xB5470917u },
		{ -28370.0f, -84750.0f, 200.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x9216D5D9u, 0x8979FB1Bu },
		{ -31170.0f, -85400.0f, 240.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x8F1BBCDCu, 0xD1310BA6u },
		{ -31520.0f, -90750.0f, 290.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xA4093822u, 0x299F31D0u },
		{ -28580.0f, -91550.0f, 260.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x082EFA98u, 0xEC4E6C89u },
		{ -28490.0f, -97590.0f, 130.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x452821E6u, 0x38D01377u },
		{ -28560.0f, -99600.0f, 130.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xBE5466CFu, 0x34E90C6Cu },
		{ -30410.0f, -108690.0f, 110.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xC0AC29B7u, 0xC97C50DDu },
		{ -28870.0f, -104630.0f, 120.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x3F84D5B5u, 0xB5470917u },
		{ -28950.0f, -114950.0f, 100.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x9216D5D9u, 0x8979FB1Bu },
		{ -28750.0f, -122540.0f, 140.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x8F1BBCDCu, 0xD1310BA6u },
		{ -31020.0f, -128250.0f, 140.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xA4093822u, 0x299F31D0u },
		{ -29790.0f, -134850.0f, 130.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x082EFA98u, 0xEC4E6C89u },
		{ -29010.0f, -139660.0f, 180.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x452821E6u, 0x38D01377u },
		{ -31440.0f, -142980.0f, 110.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xBE5466CFu, 0x34E90C6Cu },
		{ -23890.0f, -138670.0f, 260.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xC0AC29B7u, 0xC97C50DDu },
		{ -20590.0f, -146030.0f, 200.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x3F84D5B5u, 0xB5470917u },
		{ -19710.0f, -137220.0f, 250.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x9216D5D9u, 0x8979FB1Bu },
		{ -15540.0f, -139490.0f, 180.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x8F1BBCDCu, 0xD1310BA6u },
		{ -14490.0f, -145700.0f, 150.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0xA4093822u, 0x299F31D0u },
		{ -11390.0f, -137930.0f, 350.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x082EFA98u, 0xEC4E6C89u },
		{ -7510.0f, -141600.0f, 100.0f, 2, 5, 180.0f, 180.0f, STAGE2_ZOMBIE_TILE_WORLD, 1, 0x452821E6u, 0x38D01377u },
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

void Room::RecordStage2TileSequence(const Protocol::C_ENTER_GAME& pkt)
{
	if (_bHasStage2TileTypeSequence || pkt.stage2_tile_types_size() <= 0)
		return;

	vector<int32> tileTypeSequence;
	tileTypeSequence.reserve(pkt.stage2_tile_types_size());

	for (int32 tileTypeCode : pkt.stage2_tile_types())
	{
		switch (tileTypeCode)
		{
		case STAGE2_ZOMBIE_TILE_START:
		case STAGE2_ZOMBIE_TILE_STRAIGHT:
		case STAGE2_ZOMBIE_TILE_LEFT:
		case STAGE2_ZOMBIE_TILE_RIGHT:
			tileTypeSequence.push_back(tileTypeCode);
			break;
		default:
			break;
		}
	}

	if (tileTypeSequence.empty())
		return;

	_stage2TileTypeSequence = std::move(tileTypeSequence);
	_bHasStage2TileTypeSequence = true;

	cout << "[Stage2Tile] Recorded tile sequence count=" << _stage2TileTypeSequence.size()
		<< " start=" << GetStage2TileOccurrenceCount(STAGE2_ZOMBIE_TILE_START, 0)
		<< " straight=" << GetStage2TileOccurrenceCount(STAGE2_ZOMBIE_TILE_STRAIGHT, 0)
		<< " left=" << GetStage2TileOccurrenceCount(STAGE2_ZOMBIE_TILE_LEFT, 0)
		<< " right=" << GetStage2TileOccurrenceCount(STAGE2_ZOMBIE_TILE_RIGHT, 0)
		<< endl;
}

int32 Room::GetStage2TileOccurrenceCount(int32 tileTypeCode, int32 fallbackOccurrenceCount) const
{
	if (!_bHasStage2TileTypeSequence)
		return fallbackOccurrenceCount;

	return static_cast<int32>(std::count(
		_stage2TileTypeSequence.begin(),
		_stage2TileTypeSequence.end(),
		tileTypeCode));
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
	_nextStage2ZombieId = ZOMBIE_OBJECT_ID_START;
	_pendingStage2ZombieSpawns.clear();

	const ZombieSpawnGroupInfo* zombieGroups = _bHasStage2TileTypeSequence ? STAGE2_ZOMBIE_GROUPS : STAGE2_STATIC_MAP_ZOMBIE_GROUPS;
	const int32 zombieGroupCount = _bHasStage2TileTypeSequence
		? static_cast<int32>(sizeof(STAGE2_ZOMBIE_GROUPS) / sizeof(STAGE2_ZOMBIE_GROUPS[0]))
		: static_cast<int32>(sizeof(STAGE2_STATIC_MAP_ZOMBIE_GROUPS) / sizeof(STAGE2_STATIC_MAP_ZOMBIE_GROUPS[0]));
	const char* spawnMode = _bHasStage2TileTypeSequence ? "tile" : "static";

	for (int32 groupIndex = 0; groupIndex < zombieGroupCount; ++groupIndex)
	{
		const ZombieSpawnGroupInfo& groupInfo = zombieGroups[groupIndex];
		const int32 spawnColumns = std::max<int32>(1, groupInfo.columns);
		const int32 spawnRows = std::max<int32>(1, groupInfo.rows);
		const float formationYawRadians = groupInfo.formationYawDegrees * 3.14159265358979323846f / 180.0f;
		const float formationCos = cosf(formationYawRadians);
		const float formationSin = sinf(formationYawRadians);
		const int32 tileOccurrenceCount = _bHasStage2TileTypeSequence
			? GetStage2TileOccurrenceCount(groupInfo.tileTypeCode, groupInfo.tileOccurrenceCount)
			: groupInfo.tileOccurrenceCount;

		for (int32 occurrenceIndex = 0; occurrenceIndex < tileOccurrenceCount; ++occurrenceIndex)
		{
			for (int32 row = 0; row < spawnRows; ++row)
			{
				for (int32 column = 0; column < spawnColumns; ++column)
				{
					const int32 spawnIndex = occurrenceIndex * spawnRows * spawnColumns + row * spawnColumns + column;
					float localX = (static_cast<float>(column) - static_cast<float>(spawnColumns - 1) * 0.5f) * groupInfo.spacingX;
					float localY = (static_cast<float>(row) - static_cast<float>(spawnRows - 1) * 0.5f) * groupInfo.spacingY;
					if (!_bHasStage2TileTypeSequence)
					{
						// Scatter static-map groups across a disk instead of exposing the source 2x5 grid.
						const uint32 scatterSeed = groupInfo.yawSeed ^ MixStage2ZombieSeed(groupInfo.typeSeed, groupIndex + 1);
						const float scatterAngle = PickStage2ZombieUnitFloat(scatterSeed, spawnIndex * 2) * 2.0f * 3.14159265358979323846f;
						const float scatterRadiusLimit = 0.5f * (std::max)(
							groupInfo.spacingX * static_cast<float>(spawnColumns),
							groupInfo.spacingY * static_cast<float>(spawnRows));
						const float scatterRadius = sqrtf(PickStage2ZombieUnitFloat(scatterSeed, spawnIndex * 2 + 1)) * scatterRadiusLimit;
						localX = cosf(scatterAngle) * scatterRadius;
						localY = sinf(scatterAngle) * scatterRadius;
					}
					const float worldX = groupInfo.centerX + localX * formationCos - localY * formationSin;
					const float worldY = groupInfo.centerY + localX * formationSin + localY * formationCos;
					QueueStage2ZombieSpawn(
						worldX,
						worldY,
						groupInfo.centerZ,
						PickStage2ZombieYaw(groupInfo.yawSeed, spawnIndex),
						PickStage2ZombieType(groupInfo.typeSeed, spawnIndex),
						groupInfo.tileTypeCode,
						occurrenceIndex);
				}
			}
		}
	}

	reverse(_pendingStage2ZombieSpawns.begin(), _pendingStage2ZombieSpawns.end());
	ProcessPendingStage2ZombieSpawns();
	cout << "[Stage2Zombie] mode=" << spawnMode << " queued=" << _pendingStage2ZombieSpawns.size() << endl;
}

void Room::QueueStage2ZombieSpawn(
	float x,
	float y,
	float z,
	float yaw,
	Protocol::ZombieType zombieType,
	int32 tileTypeCode,
	int32 tileOccurrenceIndex)
{
	PendingStage2ZombieSpawn& pendingSpawn = _pendingStage2ZombieSpawns.emplace_back();
	pendingSpawn.zombieId = _nextStage2ZombieId++;
	pendingSpawn.x = x;
	pendingSpawn.y = y;
	pendingSpawn.z = z;
	pendingSpawn.yaw = yaw;
	pendingSpawn.encodedSpawnType = EncodeStage2ZombieSpawnType(zombieType, tileTypeCode, tileOccurrenceIndex);
}

void Room::ProcessPendingStage2ZombieSpawns()
{
	const int32 spawnCount = std::min<int32>(STAGE2_ZOMBIES_TO_SPAWN_PER_TICK, static_cast<int32>(_pendingStage2ZombieSpawns.size()));
	for (int32 index = 0; index < spawnCount; ++index)
	{
		const PendingStage2ZombieSpawn pendingSpawn = _pendingStage2ZombieSpawns.back();
		_pendingStage2ZombieSpawns.pop_back();

		MonsterRef zombie = ObjectUtils::CreateMonster(pendingSpawn.zombieId);
		zombie->posInfo->set_x(pendingSpawn.x);
		zombie->posInfo->set_y(pendingSpawn.y);
		zombie->posInfo->set_z(pendingSpawn.z);
		zombie->posInfo->set_yaw(pendingSpawn.yaw);
		zombie->posInfo->set_state(Protocol::MOVE_STATE_IDLE);
		zombie->objectInfo->mutable_pos_info()->CopyFrom(*zombie->posInfo);
		zombie->objectInfo->set_weapon_type(pendingSpawn.encodedSpawnType);

		EnterRoom(zombie, false);
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

	for (const auto& item : _players)
	{
		PlayerRef player = item.second;
		if (player == nullptr)
			continue;

		if (player->posInfo->state() == Protocol::MOVE_STATE_DEAD)
			continue;

		const Protocol::PosInfo targetPos = GetZombieTargetPosInfo(player);
		const float dx = targetPos.x() - origin.x();
		const float dy = targetPos.y() - origin.y();
		const float dz = targetPos.z() - origin.z();
		const float distSq = dx * dx + dy * dy + dz * dz;
		if (distSq > nearestDistSq)
			continue;

		nearestDistSq = distSq;
		nearestPlayer = player;
	}

	return nearestPlayer;
}

Protocol::PosInfo Room::GetZombieTargetPosInfo(const PlayerRef& player, const Protocol::PosInfo* zombiePos) const
{
	Protocol::PosInfo targetPos;
	if (player == nullptr || player->posInfo == nullptr)
		return targetPos;

	targetPos.CopyFrom(*player->posInfo);

	if (player->bIsInTruck == false || player->currentTruckId == 0)
		return targetPos;

	auto truckIt = _trucks.find(player->currentTruckId);
	if (truckIt == _trucks.end() || truckIt->second.hasTransform == false)
		return targetPos;

	// While a player is seated, their character transform can be stale or
	// relative to cargo movement. Zombies should target the vehicle body.
	targetPos.CopyFrom(truckIt->second.posInfo);
	if (zombiePos != nullptr)
	{
		float approachX = targetPos.x();
		float approachY = targetPos.y();
		GetTruckZombieApproachPoint(truckIt->second.posInfo, *zombiePos, approachX, approachY);
		targetPos.set_x(approachX);
		targetPos.set_y(approachY);
	}

	targetPos.set_object_id(player->objectInfo ? player->objectInfo->object_id() : targetPos.object_id());
	targetPos.set_state(player->posInfo->state());
	return targetPos;
}

bool Room::ShouldBroadcastZombieMove(const MonsterRef& monster, bool force, float elapsedSeconds)
{
	if (monster == nullptr)
		return false;

	if (force)
		return true;

	const uint64 zombieId = monster->objectInfo->object_id();
	ZombieMoveBroadcastState& state = _zombieMoveBroadcastStates[zombieId];
	const float broadcastElapsedSeconds = elapsedSeconds > 0.0f ? elapsedSeconds : ZOMBIE_SERVER_TICK_SECONDS;
	state.elapsedSeconds += broadcastElapsedSeconds;

	const float currentX = monster->posInfo->x();
	const float currentY = monster->posInfo->y();
	const float currentZ = monster->posInfo->z();
	const float currentYaw = monster->posInfo->yaw();
	if (!state.hasLastMove)
		return true;

	const float dx = currentX - state.lastX;
	const float dy = currentY - state.lastY;
	const float dz = currentZ - state.lastZ;
	const float distSq = dx * dx + dy * dy + dz * dz;
	const float yawDelta = fabsf(FindDeltaYawDegrees(state.lastYaw, currentYaw));
	if (distSq >= ZOMBIE_MOVE_BROADCAST_DISTANCE * ZOMBIE_MOVE_BROADCAST_DISTANCE)
		return true;

	if (yawDelta >= ZOMBIE_MOVE_BROADCAST_YAW_DELTA &&
		state.elapsedSeconds >= ZOMBIE_SERVER_TICK_SECONDS)
	{
		return true;
	}

	return state.elapsedSeconds >= ZOMBIE_MOVE_BROADCAST_INTERVAL;
}

void Room::BroadcastZombieMove(const MonsterRef& monster, bool force, float elapsedSeconds)
{
	if (monster == nullptr)
		return;

	if (!ShouldBroadcastZombieMove(monster, force, elapsedSeconds))
		return;

	Protocol::S_MOVE movePkt;
	movePkt.mutable_info()->CopyFrom(*monster->posInfo);

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(movePkt);
	Broadcast(sendBuffer);

	ZombieMoveBroadcastState& state = _zombieMoveBroadcastStates[monster->objectInfo->object_id()];
	state.lastX = monster->posInfo->x();
	state.lastY = monster->posInfo->y();
	state.lastZ = monster->posInfo->z();
	state.lastYaw = monster->posInfo->yaw();
	state.elapsedSeconds = 0.0f;
	state.hasLastMove = true;
}

vector<Room::ZombiePathPoint> Room::FindZombiePath(const Protocol::PosInfo& start, const Protocol::PosInfo& goal) const
{
	vector<ZombiePathPoint> fallbackPath;
	fallbackPath.push_back({ goal.x(), goal.y(), goal.z() });

	if (!ZOMBIE_NAV_HAS_BLOCKED_CELLS)
		return fallbackPath;

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
	unordered_map<int64, vector<MonsterRef>> separationGrid;
	separationGrid.reserve(_monsters.size());

	for (const auto& item : _monsters)
	{
		MonsterRef monster = item.second;
		if (monster == nullptr || monster->IsDead())
			continue;

		if (monster->objectInfo->weapon_type() >= 10)
			continue;

		const ZombieNavCell cell = WorldToZombieSeparationCell(monster->posInfo->x(), monster->posInfo->y());
		separationGrid[MakeZombieNavCellKey(cell.x, cell.y)].push_back(monster);
	}

	for (const auto& item : _monsters)
	{
		MonsterRef monster = item.second;
		if (monster == nullptr)
			continue;

		if (monster->IsDead())
		{
			_zombiePaths.erase(item.first);
			_zombieAiUpdateStates.erase(item.first);
			continue;
		}

		if (monster->objectInfo->weapon_type() >= 10)
		{
			// Tile-local zombies become real world zombies after a client resolves
			// their tile transform and sends a placement correction.
			continue;
		}

		PlayerRef targetPlayer = FindNearestPlayer(*monster->posInfo, ZOMBIE_AI_ACTIVE_RANGE);
		if (targetPlayer == nullptr)
		{
			_zombiePaths.erase(item.first);
			_zombieAiUpdateStates.erase(item.first);
			if (monster->posInfo->state() != Protocol::MOVE_STATE_IDLE)
			{
				monster->posInfo->set_state(Protocol::MOVE_STATE_IDLE);
				monster->objectInfo->mutable_pos_info()->CopyFrom(*monster->posInfo);
				BroadcastZombieMove(monster, true);
			}
			continue;
		}

		const auto targetTruckIt = targetPlayer->currentTruckId != 0
			? _trucks.find(targetPlayer->currentTruckId)
			: _trucks.end();
		const bool bTargetingTruck =
			targetPlayer->bIsInTruck &&
			targetTruckIt != _trucks.end() &&
			targetTruckIt->second.hasTransform;
		const Protocol::PosInfo targetPos = GetZombieTargetPosInfo(
			targetPlayer,
			bTargetingTruck ? monster->posInfo : nullptr);
		const float dx = targetPos.x() - monster->posInfo->x();
		const float dy = targetPos.y() - monster->posInfo->y();
		const float dz = targetPos.z() - monster->posInfo->z();
		const float distSq = dx * dx + dy * dy + dz * dz;

		ZombieAiUpdateState& aiUpdateState = _zombieAiUpdateStates[item.first];
		aiUpdateState.elapsedSeconds += ZOMBIE_SERVER_TICK_SECONDS;
		const float aiUpdateInterval = GetZombieAiUpdateInterval(distSq);
		if (aiUpdateState.elapsedSeconds + 0.001f < aiUpdateInterval)
			continue;

		const float aiDeltaSeconds = aiUpdateState.elapsedSeconds;
		aiUpdateState.elapsedSeconds = 0.0f;
		monster->TickCooldown(aiDeltaSeconds);

		const float attackRange = bTargetingTruck ? ZOMBIE_TRUCK_ATTACK_RANGE : ZOMBIE_ATTACK_RANGE;
		const float attackRangeSq = attackRange * attackRange;
		float separationX = 0.0f;
		float separationY = 0.0f;
		const float separationRadiusSq = ZOMBIE_SEPARATION_RADIUS * ZOMBIE_SEPARATION_RADIUS;

		const ZombieNavCell separationCell = WorldToZombieSeparationCell(monster->posInfo->x(), monster->posInfo->y());
		int32 separationNeighborCount = 0;
		for (int32 cellY = separationCell.y - 1; cellY <= separationCell.y + 1 && separationNeighborCount < ZOMBIE_SEPARATION_MAX_NEIGHBORS; ++cellY)
		{
			for (int32 cellX = separationCell.x - 1; cellX <= separationCell.x + 1 && separationNeighborCount < ZOMBIE_SEPARATION_MAX_NEIGHBORS; ++cellX)
			{
				auto gridIt = separationGrid.find(MakeZombieNavCellKey(cellX, cellY));
				if (gridIt == separationGrid.end())
					continue;

				for (const MonsterRef& otherMonster : gridIt->second)
				{
					if (separationNeighborCount >= ZOMBIE_SEPARATION_MAX_NEIGHBORS)
						break;

					if (otherMonster == nullptr ||
						otherMonster->objectInfo->object_id() == monster->objectInfo->object_id())
					{
						continue;
					}

					const float awayX = monster->posInfo->x() - otherMonster->posInfo->x();
					const float awayY = monster->posInfo->y() - otherMonster->posInfo->y();
					const float otherDistSq = awayX * awayX + awayY * awayY;
					if (otherDistSq >= separationRadiusSq)
						continue;

					if (otherDistSq <= 0.001f)
					{
						const float fallbackAngle = static_cast<float>((item.first * 37 + otherMonster->objectInfo->object_id() * 17) % 360) * (3.1415926535f / 180.0f);
						separationX += cosf(fallbackAngle);
						separationY += sinf(fallbackAngle);
						++separationNeighborCount;
						continue;
					}

					const float otherDist = sqrtf(otherDistSq);
					const float strength = (ZOMBIE_SEPARATION_RADIUS - otherDist) / ZOMBIE_SEPARATION_RADIUS;
					separationX += (awayX / otherDist) * strength;
					separationY += (awayY / otherDist) * strength;
					++separationNeighborCount;
				}
			}
		}

		if (distSq > ZOMBIE_AGGRO_RANGE * ZOMBIE_AGGRO_RANGE)
		{
			_zombiePaths.erase(item.first);
			if (monster->posInfo->state() != Protocol::MOVE_STATE_IDLE)
			{
				monster->posInfo->set_state(Protocol::MOVE_STATE_IDLE);
				monster->objectInfo->mutable_pos_info()->CopyFrom(*monster->posInfo);
				BroadcastZombieMove(monster, true);
			}

			continue;
		}

		if (distSq <= attackRangeSq)
		{
			monster->posInfo->set_state(Protocol::MOVE_STATE_IDLE);
			if (dx * dx + dy * dy > 0.001f)
			{
				const float desiredYaw = atan2f(dy, dx) * (180.0f / 3.1415926535f);
				monster->posInfo->set_yaw(StepYawTowards(
					monster->posInfo->yaw(),
					desiredYaw,
					ZOMBIE_YAW_TURN_RATE_DEGREES * aiDeltaSeconds));
			}

			const float separationSq = separationX * separationX + separationY * separationY;
			if (!bTargetingTruck && separationSq > 0.001f)
			{
				const float separationLen = sqrtf(separationSq);
				const float moveStep = ZOMBIE_MOVE_SPEED * monster->GetMoveSpeedScale() * aiDeltaSeconds;
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
			pathState.repathRemainingSeconds -= aiDeltaSeconds;

			const float targetMoveX = targetPos.x() - pathState.lastTargetX;
			const float targetMoveY = targetPos.y() - pathState.lastTargetY;
			const float targetMoveZ = targetPos.z() - pathState.lastTargetZ;
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
				pathState.waypoints = FindZombiePath(*monster->posInfo, targetPos);
				pathState.waypointIndex = 0;
				pathState.repathRemainingSeconds = ZOMBIE_PATH_RECALC_SECONDS;
				pathState.lastTargetX = targetPos.x();
				pathState.lastTargetY = targetPos.y();
				pathState.lastTargetZ = targetPos.z();
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

			ZombiePathPoint moveTarget = { targetPos.x(), targetPos.y(), targetPos.z() };
			if (pathState.waypointIndex < pathState.waypoints.size())
				moveTarget = pathState.waypoints[pathState.waypointIndex];

			const float pathDx = moveTarget.x - monster->posInfo->x();
			const float pathDy = moveTarget.y - monster->posInfo->y();
			const float pathDz = moveTarget.z - monster->posInfo->z();
			const float pathDistSq = pathDx * pathDx + pathDy * pathDy + pathDz * pathDz;
			const float distance = sqrtf(pathDistSq);
			if (distance > 0.001f)
			{
				const float pathDistSq2D = pathDx * pathDx + pathDy * pathDy;
				if (pathDistSq2D > 0.001f)
				{
					const float desiredYaw = atan2f(pathDy, pathDx) * (180.0f / 3.1415926535f);
					monster->posInfo->set_yaw(StepYawTowards(
						monster->posInfo->yaw(),
						desiredYaw,
						ZOMBIE_YAW_TURN_RATE_DEGREES * aiDeltaSeconds));
				}

				const float moveStep = ZOMBIE_MOVE_SPEED * monster->GetMoveSpeedScale() * aiDeltaSeconds;
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
			}

			monster->posInfo->set_state(Protocol::MOVE_STATE_RUN);
		}

		monster->objectInfo->mutable_pos_info()->CopyFrom(*monster->posInfo);
		BroadcastZombieMove(monster, false, aiDeltaSeconds);
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
		BroadcastZombieMove(monster, true);
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

	const float hitNormalX = pkt.hit_normal_x();
	const float hitNormalY = pkt.hit_normal_y();
	const float hitNormalZ = pkt.hit_normal_z();
	const float hitNormalLenSq = hitNormalX * hitNormalX + hitNormalY * hitNormalY + hitNormalZ * hitNormalZ;
	const bool isTruckImpactHit =
		pkt.hit_bone_name().empty() &&
		damage >= ZOMBIE_TRUCK_IMPACT_EVENT_MIN_DAMAGE &&
		hitNormalLenSq > 0.25f;
	if (isTruckImpactHit)
	{
		const float hitNormalLen = sqrtf(hitNormalLenSq);
		const float impactDirX = hitNormalX / hitNormalLen;
		const float impactDirY = hitNormalY / hitNormalLen;
		const float impactDirZ = hitNormalZ / hitNormalLen;
		const float damageAlpha = ClampFloat(
			(damage - ZOMBIE_TRUCK_IMPACT_BASE_DAMAGE) /
			(ZOMBIE_TRUCK_IMPACT_FATAL_DAMAGE - ZOMBIE_TRUCK_IMPACT_BASE_DAMAGE),
			0.0f,
			1.0f);
		const float knockbackScale = 1.0f + damageAlpha * 0.6f;

		Protocol::S_ZOMBIE_DISMEMBER impactPkt;
		impactPkt.set_zombie_id(zombieId);
		impactPkt.set_bone_name(monster->IsDead() ? "__truck_impact_ragdoll__" : "__truck_impact__");
		impactPkt.set_hit_x(pkt.hit_x());
		impactPkt.set_hit_y(pkt.hit_y());
		impactPkt.set_hit_z(pkt.hit_z());
		impactPkt.set_impulse_x(impactDirX * ZOMBIE_TRUCK_IMPACT_BASE_IMPULSE * knockbackScale * 1.2f);
		impactPkt.set_impulse_y(impactDirY * ZOMBIE_TRUCK_IMPACT_BASE_IMPULSE * knockbackScale * 1.2f);
		impactPkt.set_impulse_z(
			impactDirZ * ZOMBIE_TRUCK_IMPACT_BASE_IMPULSE * knockbackScale * 1.2f +
			ZOMBIE_TRUCK_IMPACT_BASE_IMPULSE * 0.2f * knockbackScale);

		SendBufferRef impactBuffer = ServerPacketHandler::MakeSendBuffer(impactPkt);
		Broadcast(impactBuffer, player->objectInfo ? player->objectInfo->object_id() : 0);
	}

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

		if (brokenBoneName == "head" ||
			brokenBoneName == "Head" ||
			brokenBoneName == "spine_01" ||
			brokenBoneName == "Spine")
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
	_pendingLootItemRespawns.erase(
		std::remove_if(
			_pendingLootItemRespawns.begin(),
			_pendingLootItemRespawns.end(),
			[itemId](const PendingLootItemRespawn& pendingRespawn)
			{
				return pendingRespawn.itemId == itemId;
			}),
		_pendingLootItemRespawns.end());

	Protocol::S_DESPAWN despawnPkt;
	Protocol::DespawnInfo* despawnInfo = despawnPkt.add_despawn_infos();
	despawnInfo->set_object_id(itemId);
	despawnInfo->set_object_type(Protocol::OBJECT_TYPE_ITEM);
	cout << "[PickupLootItem] broadcasting despawn for itemId=" << itemId << endl;
	SendBufferRef despawnBuffer = ServerPacketHandler::MakeSendBuffer(despawnPkt);
	Broadcast(despawnBuffer);

	if (pkt.should_respawn() && pkt.respawn_delay() > 0.0f)
	{
		cout << "[PickupLootItem] respawn request ignored for network loot itemId=" << itemId << endl;
	}
}

void Room::HandleFire(PlayerRef player, Protocol::C_FIRE pkt)
{
	UNREFERENCED_PARAMETER(pkt);

	if (player == nullptr) return;

	if (player->bIsInTruck && player->currentTruckSeatType == Protocol::TRUCK_SEAT_TURRET)
	{
		TruckState* truckState = FindTruckState(player->currentTruckId);
		if (truckState == nullptr || truckState->turretPlayerId != player->objectInfo->object_id())
			return;

		if (ConsumeMachineGunBullet(*truckState) == false)
		{
			BroadcastMachineGunAmmo(*truckState);
			return;
		}

		BroadcastMachineGunAmmo(*truckState);
	}

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
		if (seatType == Protocol::TRUCK_SEAT_TURRET)
		{
			RefreshMachineGunAmmoFromCargo(truckState);
			BroadcastMachineGunAmmo(truckState);
		}
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
	if (player == nullptr)
		return;

	const Protocol::PosInfo& incoming = pkt.info();
	const bool bPlayerInThisTruck =
		player->bIsInTruck &&
		player->currentTruckId != 0 &&
		(incoming.object_id() == 0 || incoming.object_id() == player->currentTruckId);
	const uint64 truckId = bPlayerInThisTruck ? player->currentTruckId : incoming.object_id();
	if (truckId == 0)
	{
		return;
	}

	TruckState* truckState = FindTruckState(truckId);
	if (truckState == nullptr)
		return;

	const bool bIsExplicitTruckItemUpdate = pkt.has_truck_health_repair();
	const bool bFiniteTransform =
		std::isfinite(incoming.x()) &&
		std::isfinite(incoming.y()) &&
		std::isfinite(incoming.z()) &&
		std::isfinite(incoming.yaw()) &&
		std::isfinite(incoming.pitch()) &&
		std::isfinite(incoming.roll());
	const bool bHasTruckStatePayload =
		pkt.has_truck_fuel() ||
		pkt.has_truck_health() ||
		bIsExplicitTruckItemUpdate;
	const bool bCanUseIncomingTransform =
		bFiniteTransform &&
		(!pkt.has_turret_aim() || bHasTruckStatePayload);

	if (pkt.has_truck_health() &&
		std::isfinite(pkt.truck_hp()) &&
		std::isfinite(pkt.truck_max_hp()) &&
		pkt.truck_max_hp() > 0.0f)
	{
		const float incomingMaxHp = pkt.truck_max_hp();
		const float incomingHp = (std::max)(0.0f, (std::min)(pkt.truck_hp(), incomingMaxHp));
		const bool bHasPreviousHealth = truckState->hasHealth;
		const bool bIsHealthIncrease = bHasPreviousHealth && incomingHp > truckState->hp + TRUCK_STATE_EPSILON;
		const bool bIsHealthDecrease = bHasPreviousHealth == false || incomingHp <= truckState->hp + TRUCK_STATE_EPSILON;
		const bool bIgnoreStalePostRepairHealth =
			!bIsExplicitTruckItemUpdate &&
			bHasPreviousHealth &&
			incomingHp < truckState->hp - TRUCK_STATE_EPSILON &&
			IsRecentTruckItemUpdate(truckState->lastHealthRepairUpdateTime);

		if ((bIsExplicitTruckItemUpdate && (bHasPreviousHealth == false || incomingHp >= truckState->hp - TRUCK_STATE_EPSILON)) ||
			(!bIsExplicitTruckItemUpdate && bIsHealthDecrease && !bIgnoreStalePostRepairHealth))
		{
			truckState->hasHealth = true;
			truckState->hp = incomingHp;
			truckState->maxHp = incomingMaxHp;
			if (bIsExplicitTruckItemUpdate && bIsHealthIncrease)
			{
				truckState->lastHealthRepairUpdateTime = std::chrono::steady_clock::now();
			}
		}
	}

	if (bPlayerInThisTruck == false)
	{
		if (bIsExplicitTruckItemUpdate &&
			pkt.has_truck_fuel() &&
			std::isfinite(pkt.fuel()) &&
			pkt.fuel() >= 0.0f &&
			(truckState->hasFuel == false || pkt.fuel() >= truckState->fuel - TRUCK_STATE_EPSILON))
		{
			const bool bIsFuelIncrease = truckState->hasFuel && pkt.fuel() > truckState->fuel + TRUCK_STATE_EPSILON;
			truckState->hasFuel = true;
			truckState->fuel = pkt.fuel();
			if (bIsFuelIncrease)
			{
				truckState->lastFuelItemUpdateTime = std::chrono::steady_clock::now();
			}
		}

		if (truckState->hasTransform == false && bCanUseIncomingTransform)
		{
			truckState->posInfo.CopyFrom(incoming);
			truckState->posInfo.set_object_id(truckId);
			truckState->hasTransform = true;
		}

		BroadcastTruckState(*truckState);
		return;
	}

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
		if (pkt.has_truck_fuel() &&
			std::isfinite(pkt.fuel()) &&
			pkt.fuel() >= 0.0f)
		{
			const float incomingFuel = pkt.fuel();
			if (bIsExplicitTruckItemUpdate &&
				(truckState->hasFuel == false || incomingFuel >= truckState->fuel - TRUCK_STATE_EPSILON))
			{
				const bool bIsFuelIncrease = truckState->hasFuel && incomingFuel > truckState->fuel + TRUCK_STATE_EPSILON;
				truckState->hasFuel = true;
				truckState->fuel = incomingFuel;
				if (bIsFuelIncrease)
				{
					truckState->lastFuelItemUpdateTime = std::chrono::steady_clock::now();
				}
			}
		}

		if (truckState->hasTransform == false && bCanUseIncomingTransform)
		{
			truckState->posInfo.CopyFrom(incoming);
			truckState->posInfo.set_object_id(truckId);
			truckState->hasTransform = true;
		}

		BroadcastTruckState(*truckState);
		return;
	}

	if (truckState->driverPlayerId != player->objectInfo->object_id())
		return;

	if (pkt.has_truck_fuel() && std::isfinite(pkt.fuel()) && pkt.fuel() >= 0.0f)
	{
		const float incomingFuel = pkt.fuel();
		const bool bHasPreviousFuel = truckState->hasFuel;
		const bool bIsFuelIncrease = bHasPreviousFuel && incomingFuel > truckState->fuel + TRUCK_STATE_EPSILON;
		const bool bIsFuelDecrease = bHasPreviousFuel == false || incomingFuel <= truckState->fuel + TRUCK_STATE_EPSILON;
		const bool bIgnoreStalePostRefuel =
			!bIsExplicitTruckItemUpdate &&
			bHasPreviousFuel &&
			incomingFuel < truckState->fuel - TRUCK_STATE_EPSILON &&
			IsRecentTruckItemUpdate(truckState->lastFuelItemUpdateTime);

		if ((bIsExplicitTruckItemUpdate && (bHasPreviousFuel == false || incomingFuel >= truckState->fuel - TRUCK_STATE_EPSILON)) ||
			(!bIsExplicitTruckItemUpdate && bIsFuelDecrease && !bIgnoreStalePostRefuel))
		{
			truckState->hasFuel = true;
			truckState->fuel = incomingFuel;
			if (bIsExplicitTruckItemUpdate && bIsFuelIncrease)
			{
				truckState->lastFuelItemUpdateTime = std::chrono::steady_clock::now();
			}
		}
	}

	if (bCanUseIncomingTransform == false)
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

	TruckState& truckState = GetOrCreateTruckState(truckId);
	bool bMachineGunAmmoChanged = false;

	Protocol::S_LOAD_TRUCK_ITEM loadPkt;
	loadPkt.set_player_id(player->objectInfo->object_id());
	loadPkt.set_truck_id(truckId);
	for (const int32 itemType : pkt.item_types())
	{
		loadPkt.add_item_types(itemType);
		if (itemType == MOUNTED_GUN_AMMO_ITEM_TYPE)
		{
			++truckState.mountedAmmoCount;
			bMachineGunAmmoChanged = true;
		}
	}

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(loadPkt);
	Broadcast(sendBuffer, player->objectInfo->object_id());

	if (bMachineGunAmmoChanged)
	{
		RefreshMachineGunAmmoFromCargo(truckState);
		BroadcastMachineGunAmmo(truckState);
	}
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
	_stageTransitionTruckId = truckId;
	_stageTransitionReadyPlayerIds.clear();
	_stage2TileTypeSequence.clear();
	_bHasStage2TileTypeSequence = false;

	Protocol::S_STAGE_TRANSITION transitionPkt;
	transitionPkt.set_target_level(pkt.target_level());

	SendBufferRef sendBuffer = ServerPacketHandler::MakeSendBuffer(transitionPkt);
	Broadcast(sendBuffer);
}

void Room::UpdateTick()
{
	ProcessPendingStage2ZombieSpawns();
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
	if (object == nullptr || object->objectInfo == nullptr)
		return false;

	const uint64 objectId = object->objectInfo->object_id();
	if (_objects.find(objectId) != _objects.end())
		return false;

	_objects.insert(make_pair(objectId, object));

	if (PlayerRef player = dynamic_pointer_cast<Player>(object))
	{
		_players.insert(make_pair(objectId, player));
	}
	else if (MonsterRef monster = dynamic_pointer_cast<Monster>(object))
	{
		_monsters.insert(make_pair(objectId, monster));
	}

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
	MonsterRef monster = dynamic_pointer_cast<Monster>(object);
	if (player)
	{
		_players.erase(objectId);
		_stageTransitionReadyPlayerIds.erase(objectId);
		player->room.store(weak_ptr<Room>());
		ClearPlayerTruckState(player);
	}
	else if (monster)
	{
		_monsters.erase(objectId);
	}

	if (monster || objectId >= ZOMBIE_OBJECT_ID_START)
	{
		_zombiePaths.erase(objectId);
		_zombieMoveBroadcastStates.erase(objectId);
		_zombieAiUpdateStates.erase(objectId);
	}

	_objects.erase(objectId);

	return true;
}

void Room::Broadcast(SendBufferRef sendBuffer, uint64 exceptId)
{
	for (const auto& item : _players)
	{
		PlayerRef player = item.second;
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