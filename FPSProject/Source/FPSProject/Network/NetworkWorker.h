// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "FPSProject.h"

class FSocket;

struct FPSPROJECT_API FPacketHeader
{
	FPacketHeader() : PacketSize(0), PacketID(0)
	{
	}

	FPacketHeader(uint16 PacketSize, uint16 PacketID) : PacketSize(PacketSize), PacketID(PacketID)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FPacketHeader& Header)
	{
		Ar << Header.PacketSize;
		Ar << Header.PacketID;
		return Ar;
	}

	uint16 PacketSize;
	uint16 PacketID;
};

class FPSPROJECT_API RecvWorker : public FRunnable
{
public:
	RecvWorker(FSocket* Socket, TSharedPtr<class PacketSession> Session);
	~RecvWorker();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	void Destroy();
	void StopAndWait();

private:
	bool ReceivePacket(TArray<uint8>& OutPacket);
	bool ReceiveDesiredBytes(uint8* Results, int32 Size);

protected:
	FRunnableThread* Thread = nullptr;
	FThreadSafeBool Running = true;
	FSocket* Socket;
	TWeakPtr<class PacketSession> SessionRef;
};


class FPSPROJECT_API SendWorker : public FRunnable
{
public:
	SendWorker(FSocket* Socket, TSharedPtr<class PacketSession> Session);
	~SendWorker();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	bool SendPacket(SendBufferRef SendBuffer);

	void Destroy();
	void StopAndWait();

private:
	bool SendDesiredBytes(const uint8* Buffer, int32 Size);

protected:
	FRunnableThread* Thread = nullptr;
	FThreadSafeBool Running = true;
	FSocket* Socket;
	TWeakPtr<class PacketSession> SessionRef;
};