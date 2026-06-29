#pragma once

#include "CoreMinimal.h"

class FSocket;
class PacketSession;
class SendBuffer;

class FFPSNetworkManager
{
public:
	~FFPSNetworkManager();

	bool ConnectToGameServer(const FString& IPAddress, int16 Port);
	void DisconnectFromGameServer(bool bSendLeavePacket);
	void HandleRecvPackets();
	void SendPacket(TSharedPtr<SendBuffer> SendBuffer);
	bool IsConnected() const;
	bool HasPendingRecvPackets() const;

private:
	FSocket* Socket = nullptr;
	TSharedPtr<PacketSession> GameServerSession;
};