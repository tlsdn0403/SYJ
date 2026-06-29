// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/PacketSession.h"
#include "NetworkWorker.h"
#include "Sockets.h"
#include "Common/TcpSocketBuilder.h"
#include "Serialization/ArrayWriter.h"
#include "SocketSubsystem.h"
#include "ClientPacketHandler.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

PacketSession::PacketSession(class FSocket* Socket) : Socket(Socket)
{
	ClientPacketHandler::Init();
}

PacketSession::~PacketSession()
{
	Disconnect();
}

void PacketSession::Run()
{
	if (RecvWorkerThread || SendWorkerThread)
	{
		return;
	}

	RecvWorkerThread = MakeShared<RecvWorker>(Socket, AsShared());
	SendWorkerThread = MakeShared<SendWorker>(Socket, AsShared());
}

void PacketSession::HandleRecvPackets()
{
	while (true)
	{
		TArray<uint8> Packet;
		if (RecvPacketQueue.Dequeue(OUT Packet) == false)
			break;

		PacketSessionRef ThisPtr = AsShared();
		ClientPacketHandler::HandlePacket(ThisPtr, Packet.GetData(), Packet.Num());
	}
}

void PacketSession::SendPacket(SendBufferRef SendBuffer)
{
	if (Socket == nullptr || IsConnectionClosed())
	{
		return;
	}

	SendPacketQueue.Enqueue(SendBuffer);
	if (SendWorkerThread)
	{
		SendWorkerThread->NotifyPacketQueued();
	}
}

bool PacketSession::HasPendingRecvPackets() const
{
	return !RecvPacketQueue.IsEmpty();
}

bool PacketSession::HasPendingSendPackets() const
{
	return !SendPacketQueue.IsEmpty();
}

bool PacketSession::IsConnectionClosed() const
{
	return bConnectionClosed;
}

void PacketSession::NotifyConnectionClosed()
{
	bConnectionClosed = true;
}

bool PacketSession::SendPacketNow(SendBufferRef SendBuffer, float TimeoutSeconds)
{
	if (Socket == nullptr || SendBuffer == nullptr || IsConnectionClosed())
	{
		return false;
	}

	const uint8* Buffer = SendBuffer->Buffer();
	int32 Size = SendBuffer->WriteSize();
	const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;

	while (Size > 0 && FPlatformTime::Seconds() < Deadline)
	{
		int32 BytesSent = 0;
		if (Socket->Send(Buffer, Size, BytesSent) && BytesSent > 0)
		{
			Buffer += BytesSent;
			Size -= BytesSent;
			continue;
		}

		FPlatformProcess::SleepNoStats(0.001f);
	}

	return Size == 0;
}

void PacketSession::Disconnect()
{
	bConnectionClosed = true;

	if (RecvWorkerThread)
	{
		RecvWorkerThread->StopAndWait();
		RecvWorkerThread = nullptr;
	}

	if (SendWorkerThread)
	{
		SendWorkerThread->StopAndWait();
		SendWorkerThread = nullptr;
	}

	Socket = nullptr;
}