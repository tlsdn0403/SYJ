#pragma once
#include "Creature.h"

class GameSession;
class Room;

class Player : public Creature
{
public:
	Player();
	virtual ~Player();

public:
	weak_ptr<GameSession> session;
	std::string nickname;
	bool bIsInTruck = false;
	uint64 currentTruckId = 0;
	Protocol::TruckSeatType currentTruckSeatType = Protocol::TRUCK_SEAT_NONE;
};
