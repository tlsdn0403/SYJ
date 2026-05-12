#include "pch.h"
#include <iostream>
#include <atomic>
#include "ThreadManager.h"
#include "Service.h"
#include "Session.h"
#include "ClientPacketHandler.h"

char sendData[] = "Hello World";
std::atomic<int32> GConnectedCount = 0;

constexpr int32 kDummyConnectionCount = 5000;
constexpr int32 kDummyWorkerThreadCount = 5;

class ServerSession : public PacketSession
{
public:
	~ServerSession()
	{
		cout << "~ServerSession" << endl;
	}

	virtual void OnConnected() override
	{
		GConnectedCount++;
		
		Protocol::C_ENTER_GAME pkt;
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(pkt);
		Send(sendBuffer);
	}

	virtual void OnRecvPacket(BYTE* buffer, int32 len) override
	{
		PacketSessionRef session = GetPacketSessionRef();
		PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);

		// TODO : packetId 대역 체크
		ClientPacketHandler::HandlePacket(session, buffer, len);
	}

	virtual void OnSend(int32 len) override
	{
	}

	virtual void OnDisconnected() override
	{
		GConnectedCount--;
	}
};

int main()
{
	ClientPacketHandler::Init();

	this_thread::sleep_for(1s);

	ClientServiceRef service = make_shared<ClientService>(
		NetAddress(L"127.0.0.1", 7777),
		make_shared<IocpCore>(),
		[=]() { return make_shared<ServerSession>(); }, // TODO : SessionManager 등
		kDummyConnectionCount);

	ASSERT_CRASH(service->Start());

	for (int32 i = 0; i < kDummyWorkerThreadCount; i++)
	{
		GThreadManager->Launch([=]()
			{
				while (true)
				{
					service->GetIocpCore()->Dispatch();
				}
			});
	}

	while (true)
	{
		//service->Broadcast(sendBuffer);
		cout << "[DummyClient] connected callbacks=" << GConnectedCount.load()
			<< ", service sessions=" << service->GetCurrentSessionCount()
			<< " / target=" << kDummyConnectionCount << endl;
		this_thread::sleep_for(1s);
	}

	GThreadManager->Join();

}