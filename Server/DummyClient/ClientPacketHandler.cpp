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
	cout << "[DummyClient] S_ENTER_GAME myId=" << pkt.player().object_id() << endl;
	return true;
}

bool Handle_S_LEAVE_GAME(PacketSessionRef& session, Protocol::S_LEAVE_GAME& pkt)
{
	return true;
}

bool Handle_S_SPAWN(PacketSessionRef& session, Protocol::S_SPAWN& pkt)
{
	cout << "[DummyClient] S_SPAWN count=" << pkt.players_size() << endl;
	for (const auto& objectInfo : pkt.players())
	{
		cout << "  objectId=" << objectInfo.object_id()
			<< " pos=(" << objectInfo.pos_info().x()
			<< ", " << objectInfo.pos_info().y()
			<< ", " << objectInfo.pos_info().z() << ")"
			<< endl;
	}
	return true;
}

bool Handle_S_DESPAWN(PacketSessionRef& session, Protocol::S_DESPAWN& pkt)
{
	return true;
}

bool Handle_S_MOVE(PacketSessionRef& session, Protocol::S_MOVE& pkt)
{
	if (pkt.info().object_id() >= 1000000)
	{
		cout << "[DummyClient] S_MOVE zombieId=" << pkt.info().object_id()
			<< " pos=(" << pkt.info().x()
			<< ", " << pkt.info().y()
			<< ", " << pkt.info().z()
			<< ") state=" << pkt.info().state()
			<< endl;
	}
	return true;
}

bool Handle_S_ZOMBIE_ATTACK(PacketSessionRef& session, Protocol::S_ZOMBIE_ATTACK& pkt)
{
	cout << "[DummyClient] S_ZOMBIE_ATTACK zombieId=" << pkt.zombie_id()
		<< " targetPlayerId=" << pkt.target_player_id()
		<< endl;
	return true;
}

bool Handle_S_ZOMBIE_HP(PacketSessionRef& session, Protocol::S_ZOMBIE_HP& pkt)
{
	cout << "[DummyClient] S_ZOMBIE_HP zombieId=" << pkt.zombie_id()
		<< " hp=" << pkt.hp()
		<< "/" << pkt.max_hp()
		<< endl;
	return true;
}

bool Handle_S_ZOMBIE_DIE(PacketSessionRef& session, Protocol::S_ZOMBIE_DIE& pkt)
{
	cout << "[DummyClient] S_ZOMBIE_DIE zombieId=" << pkt.zombie_id()
		<< " killerId=" << pkt.killer_id()
		<< endl;
	return true;
}

bool Handle_S_ZOMBIE_DISMEMBER(PacketSessionRef& session, Protocol::S_ZOMBIE_DISMEMBER& pkt)
{
	cout << "[DummyClient] S_ZOMBIE_DISMEMBER zombieId=" << pkt.zombie_id()
		<< " bone=" << pkt.bone_name()
		<< endl;
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

bool Handle_S_LOAD_TRUCK_ITEM(PacketSessionRef& session, Protocol::S_LOAD_TRUCK_ITEM& pkt)
{
	return true;
}

bool Handle_S_MACHINE_GUN_AMMO(PacketSessionRef& session, Protocol::S_MACHINE_GUN_AMMO& pkt)
{
	cout << "[DummyClient] S_MACHINE_GUN_AMMO truckId=" << pkt.truck_id()
		<< " total=" << pkt.total_ammo()
		<< " current=" << pkt.current_ammo()
		<< " max=" << pkt.max_ammo() << endl;
	return true;
}
bool Handle_S_TOGGLE_DOOR(PacketSessionRef& session, Protocol::S_TOGGLE_DOOR& pkt)
{
	return false;
}

bool Handle_S_ENTER_GAME_READY_COUNT(PacketSessionRef& session, Protocol::S_ENTER_GAME_READY_COUNT& pkt)
{
	return true;
}

bool Handle_S_STAGE_TIMER(PacketSessionRef& session, Protocol::S_STAGE_TIMER& pkt)
{
	return true;
}

bool Handle_S_STAGE1_ITEM_SEED(PacketSessionRef& session, Protocol::S_STAGE1_ITEM_SEED& pkt)
{
	return true;
}

bool Handle_S_RESPAWN_LOOT_ITEM(PacketSessionRef& session, Protocol::S_RESPAWN_LOOT_ITEM& pkt)
{
	return true;
}

bool Handle_S_STAGE_TRANSITION(PacketSessionRef& session, Protocol::S_STAGE_TRANSITION& pkt)
{
	cout << "[DummyClient] S_STAGE_TRANSITION targetLevel=" << pkt.target_level() << endl;
	return true;
}
