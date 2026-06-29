#include "pch.h"
#include "GameSession.h"
#include "GameSessionManager.h"
#include "ServerPacketHandler.h"
#include "Room.h"

void GameSession::OnConnected()
{
	GSessionManager.Add(static_pointer_cast<GameSession>(shared_from_this()));
}

void GameSession::OnDisconnected()
{
	GameSessionRef session = static_pointer_cast<GameSession>(shared_from_this());
	PlayerRef disconnectedPlayer = player.load();
	player.store(nullptr);

	if (GRoom)
	{
		GRoom->DoAsync(&Room::RemovePendingReadySession, session);
		if (disconnectedPlayer)
		{
			GRoom->DoAsync(&Room::HandleLeavePlayer, disconnectedPlayer);
		}
	}

	GSessionManager.Remove(session);
}

void GameSession::OnRecvPacket(BYTE* buffer, int32 len)
{
	PacketSessionRef session = GetPacketSessionRef();
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);

	// TODO : packetId 대역 체크
	ServerPacketHandler::HandlePacket(session, buffer, len);
}

void GameSession::OnSend(int32 len)
{

}