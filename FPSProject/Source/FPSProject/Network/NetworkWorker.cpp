// Fill out your copyright notice in the Description page of Project Settings.


#include "Network/NetworkWorker.h"
#include "Sockets.h"
#include "Serialization/ArrayWriter.h"
#include "PacketSession.h"

RecvWorker::RecvWorker(FSocket* Socket, TSharedPtr<class PacketSession> Session) : Socket(Socket), SessionRef(Session)
{
	Thread = FRunnableThread::Create(this, TEXT("RecvWorkerThread"));
}

RecvWorker::~RecvWorker()
{
	StopAndWait();
}

bool RecvWorker::Init()
{
	return true;
}

uint32 RecvWorker::Run()
{
	while (Running)
	{
		TArray<uint8> Packet;

		if (ReceivePacket(OUT Packet))
		{
			if (TSharedPtr<PacketSession> Session = SessionRef.Pin())
			{
				Session->RecvPacketQueue.Enqueue(Packet);
			}
		}
		else
		{
			FPlatformProcess::SleepNoStats(0.001f);
		}
	}

	return 0;
}

void RecvWorker::Exit()
{

}

void RecvWorker::Stop()
{
	Running = false;
}

void RecvWorker::Destroy()
{
	StopAndWait();
}

void RecvWorker::StopAndWait()
{
	Running = false;

	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

bool RecvWorker::ReceivePacket(TArray<uint8>& OutPacket)
{
	// Read packet header.
	const int32 HeaderSize = sizeof(FPacketHeader);
	TArray<uint8> HeaderBuffer;
	HeaderBuffer.AddZeroed(HeaderSize);

	if (ReceiveDesiredBytes(HeaderBuffer.GetData(), HeaderSize) == false)
		return false;

	// Decode packet id and size.
	FPacketHeader Header;
	{
		FMemoryReader Reader(HeaderBuffer);
		Reader << Header;
	}

	static constexpr int32 MaxPacketSize = 64 * 1024;
	if (Header.PacketSize < HeaderSize || Header.PacketSize > MaxPacketSize)
	{
		return false;
	}

	// Copy packet header.
	OutPacket = HeaderBuffer;

	// Read packet payload.
	TArray<uint8> PayloadBuffer;
	const int32 PayloadSize = Header.PacketSize - HeaderSize;
	if (PayloadSize == 0)
		return true;

	OutPacket.AddZeroed(PayloadSize);

	if (ReceiveDesiredBytes(&OutPacket[HeaderSize], PayloadSize))
		return true;

	return false;
}

bool RecvWorker::ReceiveDesiredBytes(uint8* Results, int32 Size)
{
	if (Socket == nullptr)
		return false;

	int32 Offset = 0;

	while (Running && Size > 0)
	{
		uint32 PendingDataSize = 0;
		if (Socket->HasPendingData(PendingDataSize) == false || PendingDataSize <= 0)
		{
			FPlatformProcess::SleepNoStats(0.001f);
			continue;
		}

		int32 NumRead = 0;
		const int32 BytesToRead = FMath::Min(Size, static_cast<int32>(PendingDataSize));
		if (Socket->Recv(Results + Offset, BytesToRead, OUT NumRead) == false)
			return false;

		check(NumRead <= Size);

		if (NumRead <= 0)
		{
			FPlatformProcess::SleepNoStats(0.001f);
			continue;
		}

		Offset += NumRead;
		Size -= NumRead;
	}

	return Size == 0;
}

// SendWorker
SendWorker::SendWorker(FSocket* Socket, TSharedPtr<PacketSession> Session) : Socket(Socket), SessionRef(Session)
{
	Thread = FRunnableThread::Create(this, TEXT("SendWorkerThread"));
}

SendWorker::~SendWorker()
{
	StopAndWait();
}

bool SendWorker::Init()
{
	return true;
}

uint32 SendWorker::Run()
{
	while (Running)
	{
		SendBufferRef SendBuffer;

		if (TSharedPtr<PacketSession> Session = SessionRef.Pin())
		{
			if (Session->SendPacketQueue.Dequeue(OUT SendBuffer))
			{
				SendPacket(SendBuffer);
			}
			else
			{
				FPlatformProcess::SleepNoStats(0.001f);
			}
		}
		else
		{
			FPlatformProcess::SleepNoStats(0.001f);
		}
	}

	return 0;
}

void SendWorker::Exit()
{

}

void SendWorker::Stop()
{
	Running = false;
}

bool SendWorker::SendPacket(SendBufferRef SendBuffer)
{
	if (SendDesiredBytes(SendBuffer->Buffer(), SendBuffer->WriteSize()) == false)
		return false;

	return true;
}

void SendWorker::Destroy()
{
	StopAndWait();
}

void SendWorker::StopAndWait()
{
	Running = false;

	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

bool SendWorker::SendDesiredBytes(const uint8* Buffer, int32 Size)
{
	if (Socket == nullptr)
		return false;

	while (Running && Size > 0)
	{
		int32 BytesSent = 0;
		if (Socket->Send(Buffer, Size, BytesSent) == false || BytesSent <= 0)
		{
			FPlatformProcess::SleepNoStats(0.001f);
			continue;
		}

		Size -= BytesSent;
		Buffer += BytesSent;
	}

	return Size == 0;
}