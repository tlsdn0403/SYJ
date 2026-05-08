#include "pch.h"
#include "ClientPacketHandler.h"
#include "BufferReader.h"

PacketHandlerFunc GPacketHandler[UINT16_MAX];

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	return false;
}

bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt)
{
	return true;
}

bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt)
{
	return true;
}

bool Handle_S_LEAVE_GAME(PacketSessionRef& session, Protocol::S_LEAVE_GAME& pkt)
{
	return true;
}

bool Handle_S_SPAWN(PacketSessionRef& session, Protocol::S_SPAWN& pkt)
{
	return true;
}

bool Handle_S_DESPAWN(PacketSessionRef& session, Protocol::S_DESPAWN& pkt)
{
	return true;
}

bool Handle_S_MOVE(PacketSessionRef& session, Protocol::S_MOVE& pkt)
{
	return true;
}

bool Handle_S_CHAT(PacketSessionRef& session, Protocol::S_CHAT& pkt)
{
	return true;
}

bool Handle_S_EQUIP_WEAPON(PacketSessionRef& session, Protocol::S_EQUIP_WEAPON& pkt)
{
	return true;
}

bool Handle_S_SPAWN_ITEM(PacketSessionRef& session, Protocol::S_SPAWN_ITEM& pkt)
{
	return true;
}

bool Handle_S_FIRE(PacketSessionRef& session, Protocol::S_FIRE& pkt)
{
	return false;
}

bool Handle_S_ENTER_TRUCK(PacketSessionRef& session, Protocol::S_ENTER_TRUCK& pkt)
{
	return false;
}

bool Handle_S_EXIT_TRUCK(PacketSessionRef& session, Protocol::S_EXIT_TRUCK& pkt)
{
	return false;
}

bool Handle_S_TRUCK_MOVE(PacketSessionRef& session, Protocol::S_TRUCK_MOVE& pkt)
{
	return false;
}

bool Handle_S_TOGGLE_DOOR(PacketSessionRef& session, Protocol::S_TOGGLE_DOOR& pkt)
{
	return false;
}