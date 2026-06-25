// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct PacketHeader
{
	uint16 size;
	uint16 id; // Protocol id. For example: 1=login, 2=move request.
};

class SendBuffer : public TSharedFromThis<SendBuffer>
{
public:
	SendBuffer(int32 bufferSize);
	~SendBuffer();

	BYTE* Buffer() { return _buffer.GetData(); }
	int32 WriteSize() { return _writeSize; }
	int32 Capacity() { return static_cast<int32>(_buffer.Num()); }

	void CopyData(void* data, int32 len);
	void Close(uint32 writeSize);

private:
	TArray<BYTE>	_buffer;
	int32			_writeSize = 0;
};

#define USING_SHARED_PTR(name)	using name##Ref = TSharedPtr<class name>;

USING_SHARED_PTR(Session);
USING_SHARED_PTR(PacketSession);
USING_SHARED_PTR(SendBuffer);

#include "ClientPacketHandler.h"
#include "FPSProjectGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace FPSProjectStableActorIdUtils
{
	inline FString StripPiePrefix(const FString& InValue)
	{
		FString Result = InValue;
		const FString Prefix = TEXT("UEDPIE_");

		int32 PrefixIndex = Result.Find(Prefix);
		while (PrefixIndex != INDEX_NONE)
		{
			int32 SuffixIndex = PrefixIndex + Prefix.Len();
			while (SuffixIndex < Result.Len() && FChar::IsDigit(Result[SuffixIndex]))
			{
				++SuffixIndex;
			}

			if (SuffixIndex < Result.Len() && Result[SuffixIndex] == TEXT('_'))
			{
				Result.RemoveAt(PrefixIndex, SuffixIndex - PrefixIndex + 1, EAllowShrinking::No);
			}
			else
			{
				break;
			}

			PrefixIndex = Result.Find(Prefix);
		}

		return Result;
	}

	inline FString BuildStableActorKey(const AActor* Actor)
	{
		if (Actor == nullptr)
		{
			return FString();
		}

		const FVector Location = Actor->GetActorLocation();
		const FIntVector QuantizedLocation(
			FMath::RoundToInt(Location.X),
			FMath::RoundToInt(Location.Y),
			FMath::RoundToInt(Location.Z));

		return FString::Printf(
			TEXT("%s:%d:%d:%d"),
			*StripPiePrefix(Actor->GetClass()->GetPathName()),
			QuantizedLocation.X,
			QuantizedLocation.Y,
			QuantizedLocation.Z);
	}
}

#define SEND_PACKET(Pkt) UFPSProjectGameInstance::SendPacketStatic(ClientPacketHandler::MakeSendBuffer(Pkt))
