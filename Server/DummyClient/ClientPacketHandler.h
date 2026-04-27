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
	PKT_C_CHAT = 1010,
	PKT_S_CHAT = 1011,
	PKT_C_EQUIP_WEAPON = 1012,
	PKT_S_EQUIP_WEAPON = 1013,
	PKT_S_SPAWN_ITEM = 1014,
	PKT_C_FIRE = 1015,
	PKT_S_FIRE = 1016,
	PKT_C_ENTER_TRUCK = 1017,
	PKT_S_ENTER_TRUCK = 1018,
	PKT_C_EXIT_TRUCK = 1019,
	PKT_S_EXIT_TRUCK = 1020,
	PKT_C_TRUCK_MOVE = 1021,
	PKT_S_TRUCK_MOVE = 1022,
	PKT_C_TOGGLE_DOOR = 1023,
	PKT_S_TOGGLE_DOOR = 1024,
};

// Custom Handlers
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len);
bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt);
bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt);
bool Handle_S_LEAVE_GAME(PacketSessionRef& session, Protocol::S_LEAVE_GAME& pkt);
bool Handle_S_SPAWN(PacketSessionRef& session, Protocol::S_SPAWN& pkt);
bool Handle_S_DESPAWN(PacketSessionRef& session, Protocol::S_DESPAWN& pkt);
bool Handle_S_MOVE(PacketSessionRef& session, Protocol::S_MOVE& pkt);
bool Handle_S_CHAT(PacketSessionRef& session, Protocol::S_CHAT& pkt);
bool Handle_S_EQUIP_WEAPON(PacketSessionRef& session, Protocol::S_EQUIP_WEAPON& pkt);
bool Handle_S_SPAWN_ITEM(PacketSessionRef& session, Protocol::S_SPAWN_ITEM& pkt);
bool Handle_S_FIRE(PacketSessionRef& session, Protocol::S_FIRE& pkt);
bool Handle_S_ENTER_TRUCK(PacketSessionRef& session, Protocol::S_ENTER_TRUCK& pkt);
bool Handle_S_EXIT_TRUCK(PacketSessionRef& session, Protocol::S_EXIT_TRUCK& pkt);
bool Handle_S_TRUCK_MOVE(PacketSessionRef& session, Protocol::S_TRUCK_MOVE& pkt);
bool Handle_S_TOGGLE_DOOR(PacketSessionRef& session, Protocol::S_TOGGLE_DOOR& pkt);

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
		GPacketHandler[PKT_S_CHAT] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_CHAT>(Handle_S_CHAT, session, buffer, len); };
		GPacketHandler[PKT_S_EQUIP_WEAPON] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_EQUIP_WEAPON>(Handle_S_EQUIP_WEAPON, session, buffer, len); };
		GPacketHandler[PKT_S_SPAWN_ITEM] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_SPAWN_ITEM>(Handle_S_SPAWN_ITEM, session, buffer, len); };
		GPacketHandler[PKT_S_FIRE] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_FIRE>(Handle_S_FIRE, session, buffer, len); };
		GPacketHandler[PKT_S_ENTER_TRUCK] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_ENTER_TRUCK>(Handle_S_ENTER_TRUCK, session, buffer, len); };
		GPacketHandler[PKT_S_EXIT_TRUCK] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_EXIT_TRUCK>(Handle_S_EXIT_TRUCK, session, buffer, len); };
		GPacketHandler[PKT_S_TRUCK_MOVE] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_TRUCK_MOVE>(Handle_S_TRUCK_MOVE, session, buffer, len); };
		GPacketHandler[PKT_S_TOGGLE_DOOR] = [](PacketSessionRef& session, BYTE* buffer, int32 len) { return HandlePacket<Protocol::S_TOGGLE_DOOR>(Handle_S_TOGGLE_DOOR, session, buffer, len); };
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
	static SendBufferRef MakeSendBuffer(Protocol::C_CHAT& pkt) { return MakeSendBuffer(pkt, PKT_C_CHAT); }
	static SendBufferRef MakeSendBuffer(Protocol::C_EQUIP_WEAPON& pkt) { return MakeSendBuffer(pkt, PKT_C_EQUIP_WEAPON); }
	static SendBufferRef MakeSendBuffer(Protocol::C_FIRE& pkt) { return MakeSendBuffer(pkt, PKT_C_FIRE); }
	static SendBufferRef MakeSendBuffer(Protocol::C_ENTER_TRUCK& pkt) { return MakeSendBuffer(pkt, PKT_C_ENTER_TRUCK); }
	static SendBufferRef MakeSendBuffer(Protocol::C_EXIT_TRUCK& pkt) { return MakeSendBuffer(pkt, PKT_C_EXIT_TRUCK); }
	static SendBufferRef MakeSendBuffer(Protocol::C_TRUCK_MOVE& pkt) { return MakeSendBuffer(pkt, PKT_C_TRUCK_MOVE); }
	static SendBufferRef MakeSendBuffer(Protocol::C_TOGGLE_DOOR& pkt) { return MakeSendBuffer(pkt, PKT_C_TOGGLE_DOOR); }

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