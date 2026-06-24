#include "FPSNetworkManager.h"
#include "ClientPacketHandler.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Networking.h"
#include "Network/PacketSession.h"
#include "Protocol.pb.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

FFPSNetworkManager::~FFPSNetworkManager()
{
	DisconnectFromGameServer(false);
}

bool FFPSNetworkManager::ConnectToGameServer(const FString& IPAddress, int16 Port)
{
	DisconnectFromGameServer(false);

	FIPv4Address Ip;
	if (FIPv4Address::Parse(IPAddress, Ip) == false)
	{
		return false;
	}

	Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(TEXT("Stream"), TEXT("Client Socket"));
	if (Socket == nullptr)
	{
		return false;
	}

	TSharedRef<FInternetAddr> InternetAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	InternetAddr->SetIp(Ip.Value);
	InternetAddr->SetPort(Port);

	if (Socket->Connect(*InternetAddr) == false)
	{
		DisconnectFromGameServer(false);
		return false;
	}

	Socket->SetNonBlocking(true);

	GameServerSession = MakeShared<PacketSession>(Socket);
	GameServerSession->Run();
	return true;
}

void FFPSNetworkManager::DisconnectFromGameServer(bool bSendLeavePacket)
{
	if (Socket && GameServerSession)
	{
		if (bSendLeavePacket)
		{
			Protocol::C_LEAVE_GAME LeavePkt;
			if (GameServerSession->SendPacketNow(ClientPacketHandler::MakeSendBuffer(LeavePkt)) == false)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Network] Failed to send C_LEAVE_GAME before disconnect"));
			}
		}

		GameServerSession->Disconnect();
		GameServerSession = nullptr;
	}

	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}

void FFPSNetworkManager::HandleRecvPackets()
{
	if (!IsConnected())
	{
		return;
	}

	GameServerSession->HandleRecvPackets();
}

void FFPSNetworkManager::SendPacket(TSharedPtr<SendBuffer> SendBuffer)
{
	if (!IsConnected())
	{
		return;
	}

	GameServerSession->SendPacket(SendBuffer);
}

bool FFPSNetworkManager::IsConnected() const
{
	return Socket != nullptr && GameServerSession != nullptr;
}