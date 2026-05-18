#pragma once
#include "Protocol.pb.h"

#if UE_BUILD_DEBUG + UE_BUILD_DEVELOPMENT + UE_BUILD_TEST + UE_BUILD_SHIPPING >= 1
#include "FPSProject.h"
#endif

using PacketHandlerFunc = std::function<bool(PacketSessionRef&, BYTE*, int32)>;
extern PacketHandlerFunc GPacketHandler[UINT16_MAX];

enum : uint16
{
	PKT_C_LOGIN = 1000,
	PKT_S_LOGIN = 1001,
	PKT_C_ENTER_GAME = 1002,
	PKT_S_ENTER_GAME = 1003,
	PKT_C_LEAVE_GAME = 1004,
	PKT_S_LEAVE_GAME = 1005,
	PKT_S_SPAWN = 1006,
	PKT_S_DESPAWN = 1007,
	PKT_C_MOVE = 1008,
	PKT_S_MOVE = 1009,
	PKT_C_HIT_ZOMBIE = 1010,
	PKT_S_ZOMBIE_ATTACK = 1011,
	PKT_S_ZOMBIE_HP = 1012,
	PKT_S_ZOMBIE_DIE = 1013,
	PKT_C_CHAT = 1014,
	PKT_S_CHAT = 1015,
	PKT_C_EQUIP_WEAPON = 1016,
	PKT_C_PICKUP_LOOT_ITEM = 1017,
	PKT_S_EQUIP_WEAPON = 1018,
	PKT_S_SPAWN_ITEM = 1019,
	PKT_C_FIRE = 1020,
	PKT_S_FIRE = 1021,
	PKT_C_ENTER_TRUCK = 1022,
	PKT_S_ENTER_TRUCK = 1023,
	PKT_C_EXIT_TRUCK = 1024,
	PKT_S_EXIT_TRUCK = 1025,
	PKT_C_TRUCK_MOVE = 1026,
	PKT_S_TRUCK_MOVE = 1027,
	PKT_C_LOAD_TRUCK_ITEM = 1028,
	PKT_S_LOAD_TRUCK_ITEM = 1029,
	PKT_C_TOGGLE_DOOR = 1030,
	PKT_S_TOGGLE_DOOR = 1031,
	PKT_S_ENTER_GAME_READY_COUNT = 1032,
	PKT_S_STAGE_TIMER = 1033,
	PKT_S_STAGE1_ITEM_SEED = 1034,
	PKT_S_RESPAWN_LOOT_ITEM = 1035,
	PKT_C_STAGE_TRANSITION_REQUEST = 1036,
	PKT_S_STAGE_TRANSITION = 1037,
};

// Custom Handlers
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len);
bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt);
bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt);
bool Handle_S_LEAVE_GAME(PacketSessionRef& session, Protocol::S_LEAVE_GAME& pkt);
bool Handle_S_SPAWN(PacketSessionRef& session, Protocol::S_SPAWN& pkt);
bool Handle_S_DESPAWN(PacketSessionRef& session, Protocol::S_DESPAWN& pkt);
bool Handle_S_MOVE(PacketSessionRef& session, Protocol::S_MOVE& pkt);
bool Handle_S_ZOMBIE_ATTACK(PacketSessionRef& session, Protocol::S_ZOMBIE_ATTACK& pkt);
bool Handle_S_ZOMBIE_HP(PacketSessionRef& session, Protocol::S_ZOMBIE_HP& pkt);
bool Handle_S_ZOMBIE_DIE(PacketSessionRef& session, Protocol::S_ZOMBIE_DIE& pkt);
bool Handle_S_CHAT(PacketSessionRef& session, Protocol::S_CHAT& pkt);
bool Handle_S_EQUIP_WEAPON(PacketSessionRef& session, Protocol::S_EQUIP_WEAPON& pkt);
bool Handle_S_SPAWN_ITEM(PacketSessionRef& session, Protocol::S_SPAWN_ITEM& pkt);
bool Handle_S_FIRE(PacketSessionRef& session, Protocol::S_FIRE& pkt);
bool Handle_S_ENTER_TRUCK(PacketSessionRef& session, Protocol::S_ENTER_TRUCK& pkt);
bool Handle_S_EXIT_TRUCK(PacketSessionRef& session, Protocol::S_EXIT_TRUCK& pkt);
bool Handle_S_TRUCK_MOVE(PacketSessionRef& session, Protocol::S_TRUCK_MOVE& pkt);
bool Handle_S_LOAD_TRUCK_ITEM(PacketSessionRef& session, Protocol::S_LOAD_TRUCK_ITEM& pkt);
bool Handle_S_TOGGLE_DOOR(PacketSessionRef& session, Protocol::S_TOGGLE_DOOR& pkt);
bool Handle_S_ENTER_GAME_READY_COUNT(PacketSessionRef& session, Protocol::S_ENTER_GAME_READY_COUNT& pkt);
bool Handle_S_STAGE_TIMER(PacketSessionRef& session, Protocol::S_STAGE_TIMER& pkt);
bool Handle_S_STAGE1_ITEM_SEED(PacketSessionRef& session, Protocol::S_STAGE1_ITEM_SEED& pkt);
bool Handle_S_RESPAWN_LOOT_ITEM(PacketSessionRef& session, Protocol::S_RESPAWN_LOOT_ITEM& pkt);
bool Handle_S_STAGE_TRANSITION(PacketSessionRef& session, Protocol::S_STAGE_TRANSITION& pkt);

class ClientPacketHandler
{
public:
	static void Init()
	{
		for (int32 i = 0; i < UINT16_MAX; i++)
			GPacketHandler[i] = Handle_INVALID;
		GPacketHandler[PKT_S_LOGIN] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_LOGIN>(Handle_S_LOGIN, session, buffer, len); };
		GPacketHandler[PKT_S_ENTER_GAME] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_ENTER_GAME>(Handle_S_ENTER_GAME, session, buffer, len); };
		GPacketHandler[PKT_S_LEAVE_GAME] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_LEAVE_GAME>(Handle_S_LEAVE_GAME, session, buffer, len); };
		GPacketHandler[PKT_S_SPAWN] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_SPAWN>(Handle_S_SPAWN, session, buffer, len); };
		GPacketHandler[PKT_S_DESPAWN] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_DESPAWN>(Handle_S_DESPAWN, session, buffer, len); };
		GPacketHandler[PKT_S_MOVE] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_MOVE>(Handle_S_MOVE, session, buffer, len); };
		GPacketHandler[PKT_S_ZOMBIE_ATTACK] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_ZOMBIE_ATTACK>(Handle_S_ZOMBIE_ATTACK, session, buffer, len); };
		GPacketHandler[PKT_S_ZOMBIE_HP] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_ZOMBIE_HP>(Handle_S_ZOMBIE_HP, session, buffer, len); };
		GPacketHandler[PKT_S_ZOMBIE_DIE] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_ZOMBIE_DIE>(Handle_S_ZOMBIE_DIE, session, buffer, len); };
		GPacketHandler[PKT_S_CHAT] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_CHAT>(Handle_S_CHAT, session, buffer, len); };
		GPacketHandler[PKT_S_EQUIP_WEAPON] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_EQUIP_WEAPON>(Handle_S_EQUIP_WEAPON, session, buffer, len); };
		GPacketHandler[PKT_S_SPAWN_ITEM] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_SPAWN_ITEM>(Handle_S_SPAWN_ITEM, session, buffer, len); };
		GPacketHandler[PKT_S_FIRE] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_FIRE>(Handle_S_FIRE, session, buffer, len); };
		GPacketHandler[PKT_S_ENTER_TRUCK] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_ENTER_TRUCK>(Handle_S_ENTER_TRUCK, session, buffer, len); };
		GPacketHandler[PKT_S_EXIT_TRUCK] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_EXIT_TRUCK>(Handle_S_EXIT_TRUCK, session, buffer, len); };
		GPacketHandler[PKT_S_TRUCK_MOVE] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_TRUCK_MOVE>(Handle_S_TRUCK_MOVE, session, buffer, len); };
		GPacketHandler[PKT_S_LOAD_TRUCK_ITEM] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_LOAD_TRUCK_ITEM>(Handle_S_LOAD_TRUCK_ITEM, session, buffer, len); };
		GPacketHandler[PKT_S_TOGGLE_DOOR] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_TOGGLE_DOOR>(Handle_S_TOGGLE_DOOR, session, buffer, len); };
		GPacketHandler[PKT_S_ENTER_GAME_READY_COUNT] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_ENTER_GAME_READY_COUNT>(Handle_S_ENTER_GAME_READY_COUNT, session, buffer, len); };
		GPacketHandler[PKT_S_STAGE_TIMER] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_STAGE_TIMER>(Handle_S_STAGE_TIMER, session, buffer, len); };
		GPacketHandler[PKT_S_STAGE1_ITEM_SEED] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_STAGE1_ITEM_SEED>(Handle_S_STAGE1_ITEM_SEED, session, buffer, len); };
		GPacketHandler[PKT_S_RESPAWN_LOOT_ITEM] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_RESPAWN_LOOT_ITEM>(Handle_S_RESPAWN_LOOT_ITEM, session, buffer, len); };
		GPacketHandler[PKT_S_STAGE_TRANSITION] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_STAGE_TRANSITION>(Handle_S_STAGE_TRANSITION, session, buffer, len); };
	}

	static bool HandlePacket(PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
		return GPacketHandler[header->id](session, buffer, len);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_LOGIN& pkt) { return MakeSendBuffer(pkt, PKT_C_LOGIN); }
	static SendBufferRef MakeSendBuffer(Protocol::C_ENTER_GAME& pkt) { return MakeSendBuffer(pkt, PKT_C_ENTER_GAME); }
	static SendBufferRef MakeSendBuffer(Protocol::C_LEAVE_GAME& pkt) { return MakeSendBuffer(pkt, PKT_C_LEAVE_GAME); }
	static SendBufferRef MakeSendBuffer(Protocol::C_MOVE& pkt) { return MakeSendBuffer(pkt, PKT_C_MOVE); }
	static SendBufferRef MakeSendBuffer(Protocol::C_HIT_ZOMBIE& pkt) { return MakeSendBuffer(pkt, PKT_C_HIT_ZOMBIE); }
	static SendBufferRef MakeSendBuffer(Protocol::C_CHAT& pkt) { return MakeSendBuffer(pkt, PKT_C_CHAT); }
	static SendBufferRef MakeSendBuffer(Protocol::C_EQUIP_WEAPON& pkt) { return MakeSendBuffer(pkt, PKT_C_EQUIP_WEAPON); }
	static SendBufferRef MakeSendBuffer(Protocol::C_PICKUP_LOOT_ITEM& pkt) { return MakeSendBuffer(pkt, PKT_C_PICKUP_LOOT_ITEM); }
	static SendBufferRef MakeSendBuffer(Protocol::C_FIRE& pkt) { return MakeSendBuffer(pkt, PKT_C_FIRE); }
	static SendBufferRef MakeSendBuffer(Protocol::C_ENTER_TRUCK& pkt) { return MakeSendBuffer(pkt, PKT_C_ENTER_TRUCK); }
	static SendBufferRef MakeSendBuffer(Protocol::C_EXIT_TRUCK& pkt) { return MakeSendBuffer(pkt, PKT_C_EXIT_TRUCK); }
	static SendBufferRef MakeSendBuffer(Protocol::C_TRUCK_MOVE& pkt) { return MakeSendBuffer(pkt, PKT_C_TRUCK_MOVE); }
	static SendBufferRef MakeSendBuffer(Protocol::C_LOAD_TRUCK_ITEM& pkt) { return MakeSendBuffer(pkt, PKT_C_LOAD_TRUCK_ITEM); }
	static SendBufferRef MakeSendBuffer(Protocol::C_TOGGLE_DOOR& pkt) { return MakeSendBuffer(pkt, PKT_C_TOGGLE_DOOR); }
	static SendBufferRef MakeSendBuffer(Protocol::C_STAGE_TRANSITION_REQUEST& pkt) { return MakeSendBuffer(pkt, PKT_C_STAGE_TRANSITION_REQUEST); }

private:
	template<typename PacketType, typename ProcessFunc>
	static bool HandlePacket(ProcessFunc func, PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketType pkt;
		if (pkt.ParseFromArray(buffer + sizeof(PacketHeader), len - sizeof(PacketHeader)) == false)
			return false;

		return func(session, pkt);
	}

	template<typename T>
	static SendBufferRef MakeSendBuffer(T& pkt, uint16 pktId)
	{
		const uint16 dataSize = static_cast<uint16>(pkt.ByteSizeLong());
		const uint16 packetSize = dataSize + sizeof(PacketHeader);

#if UE_BUILD_DEBUG + UE_BUILD_DEVELOPMENT + UE_BUILD_TEST + UE_BUILD_SHIPPING >= 1
		SendBufferRef sendBuffer = MakeShared<SendBuffer>(packetSize);
#else
		SendBufferRef sendBuffer = make_shared<SendBuffer>(packetSize);
#endif

		PacketHeader* header = reinterpret_cast<PacketHeader*>(sendBuffer->Buffer());
		header->size = packetSize;
		header->id = pktId;
		pkt.SerializeToArray(&header[1], dataSize);
		sendBuffer->Close(packetSize);

		return sendBuffer;
	}
};