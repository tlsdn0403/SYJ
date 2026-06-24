#pragma once

#include "CoreMinimal.h"
#include "Protocol.pb.h"

class UFPSProjectGameInstance;

class FFPSSpawnManager
{
public:
	explicit FFPSSpawnManager(UFPSProjectGameInstance& InOwner);

	void ProcessSpawnObject(const Protocol::ObjectInfo& ObjectInfo, bool bIsMine);

private:
	struct FPlayerSpawnContext
	{
		uint64 ObjectId = 0;
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		Protocol::PosInfo PosInfo;
		bool bUsedStage2SpawnTransform = false;
	};

	bool TryBuildPlayerSpawnContext(UWorld* World, const Protocol::ObjectInfo& ObjectInfo, FPlayerSpawnContext& OutContext) const;
	void SpawnZombie(UWorld* World, const Protocol::ObjectInfo& ObjectInfo);
	void SpawnLocalPlayer(UWorld* World, const FPlayerSpawnContext& SpawnContext);
	void SpawnRemotePlayer(UWorld* World, const FPlayerSpawnContext& SpawnContext);

	UFPSProjectGameInstance& Owner;
};