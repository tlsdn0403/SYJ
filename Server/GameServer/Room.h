#pragma once
#include "JobQueue.h"

class Room : public JobQueue
{
public:
	Room();
	virtual ~Room();

public:
	bool EnterRoom(ObjectRef object, bool randPos = true);
	bool LeaveRoom(ObjectRef object);

	bool HandleEnterPlayer(PlayerRef player);
	bool HandleLeavePlayer(PlayerRef player);
	void HandleMove(PlayerRef player, Protocol::C_MOVE pkt);
	void HandleEquipWeapon(PlayerRef player, Protocol::C_EQUIP_WEAPON pkt);
	void HandleFire(PlayerRef player, Protocol::C_FIRE pkt);
	void HandleEnterTruck(PlayerRef player, Protocol::C_ENTER_TRUCK pkt);
	void HandleExitTruck(PlayerRef player, Protocol::C_EXIT_TRUCK pkt);
	void HandleTruckMove(PlayerRef player, Protocol::C_TRUCK_MOVE pkt);
	void HandleToggleDoor(PlayerRef player, Protocol::C_TOGGLE_DOOR pkt);

public:
	void UpdateTick();

	RoomRef GetRoomRef();

private:
	bool AddObject(ObjectRef object);
	bool RemoveObject(uint64 objectId);

private:
	void Broadcast(SendBufferRef sendBuffer, uint64 exceptId = 0);

	struct TruckState
	{
		Protocol::PosInfo posInfo;
		uint64 driverPlayerId = 0;
		uint64 cargoPlayerId = 0;
		uint64 turretPlayerId = 0;
	};

	TruckState* FindTruckState(uint64 truckId);
	TruckState& GetOrCreateTruckState(uint64 truckId);
	bool IsTruckSeatOccupied(const TruckState& truckState, Protocol::TruckSeatType seatType) const;
	void SetTruckSeatOccupant(TruckState& truckState, Protocol::TruckSeatType seatType, uint64 playerId);
	void ClearTruckSeatOccupant(TruckState& truckState, Protocol::TruckSeatType seatType, uint64 playerId);
	void ClearPlayerTruckState(PlayerRef player);
	void ForceExitTruck(PlayerRef player);

private:
	unordered_map<uint64, ObjectRef> _objects;
	unordered_map<uint64, TruckState> _trucks;
	unordered_map<uint64, bool> _doors;
};

extern RoomRef GRoom;
